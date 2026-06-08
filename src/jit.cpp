#include "jit.hpp"
#include "cell.hpp"

#include <sys/mman.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <array>
#include <cassert>

// Runtime helpers callable from JIT'd code (extern "C" so the symbol
// is reachable via raw absolute call from emitted machine code)

extern "C" void btf_jit_putc(std::int8_t c) {
    std::cout.put(static_cast<char>(static_cast<unsigned char>(c)));
}

namespace {
constexpr std::size_t PER_OP   = 32;    // worst-case bytes one op emits
constexpr std::size_t OVERHEAD = 256;   // prologue + epilogue + slack
constexpr std::size_t PAGE     = 4096;

// LUT keyed by raw int8 cell value (re-read as uint8 index).  Pre-encodes
// trunc-toward-zero signed /3 for the full 256-entry domain; cells stay in
// [-121, 121] but filling all 256 keeps the lookup unconditional.
constexpr auto make_div3_lut() {
    std::array<std::int8_t, 256> a{};
    for (int i = 0; i < 256; ++i)
        a[i] = static_cast<std::int8_t>(static_cast<std::int8_t>(i) / 3);
    return a;
}
constexpr auto btf_div3_lut = make_div3_lut();
}

// The code buffer is mapped per compile() at the exact size the IR needs
// (see below), not reserved up front.  Keeps footprint at a few KB-MB for the
// loops we actually JIT instead of a fixed 128 MB reservation per instance.
BtfJit::BtfJit() : code_(nullptr), code_cap_(0), code_pos_(0) {}

BtfJit::~BtfJit() {
    if (code_) ::munmap(code_, code_cap_);
}

bool BtfJit::can_compile(const std::vector<Op>& ops) {
    // JIT exists to accelerate MUL3/DIV3.  We reject:
    //   - SCAN/SIGN/trit-I/O: interp wins (memchr SIMD, complex codegen).
    //   - peephole-fused drains (MOVE_ADD/SUB, ADD_AT/SUB_AT): no MUL3/DIV3
    //     content; JIT'ing them yields no speedup over interp.
    // Worthiness gate (measured): the JIT only pays off when codegen amortizes
    // over many executions, i.e. a loop.  A straight-line program runs each op
    // once: compile cost (e.g. 7ms for 400k ops) then a single pass the
    // interpreter does just as fast.  So require BOTH some MUL3/DIV3 AND a loop
    // (a JZ).  No loop => decline and let the interpreter run it.
    bool has_arith = false;
    bool has_loop  = false;
    for (const Op& op : ops) {
        switch (op.kind) {
            case Op::SCAN: case Op::SIGN:
            case Op::PUTTR: case Op::GETTR: case Op::GETC:
            case Op::MOVE_ADD: case Op::MOVE_SUB:
            case Op::ADD_AT:   case Op::SUB_AT:
            case Op::REG_STORE: case Op::REG_PUT:
            case Op::ANC_STORE: case Op::ANC_RECALL:
                return false;
            case Op::MUL3: case Op::DIV3:
                has_arith = true;
                break;
            case Op::JZ:
                has_loop = true;
                break;
            default: break;
        }
    }
    return has_arith && has_loop;
}

BtfJit::JitFn BtfJit::compile(const std::vector<Op>& ops) {
    // Map exactly what this IR needs (worst case PER_OP bytes/op), rounded to a
    // page.  Drop any previous mapping first.  No per-byte bounds checks needed
    // in the hot emit path: the buffer is sized to the worst case up front.
    std::size_t need = (ops.size() * PER_OP + OVERHEAD + PAGE - 1) & ~(PAGE - 1);
    if (code_) { ::munmap(code_, code_cap_); code_ = nullptr; code_cap_ = 0; }
    code_ = static_cast<std::uint8_t*>(::mmap(
        nullptr, need, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (code_ == MAP_FAILED) {
        code_ = nullptr;
        throw std::runtime_error("BtfJit: mmap failed");
    }
    code_cap_ = need;
    code_pos_ = 0;

    auto emit_b   = [&](std::uint8_t b) { code_[code_pos_++] = b; };
    auto emit_w32 = [&](std::uint32_t v) {
        emit_b(v & 0xFF);
        emit_b((v >>  8) & 0xFF);
        emit_b((v >> 16) & 0xFF);
        emit_b((v >> 24) & 0xFF);
    };
    auto emit_w64 = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) emit_b((v >> (i * 8)) & 0xFF);
    };
    auto emit = [&](std::initializer_list<std::uint8_t> bs) {
        for (auto b : bs) emit_b(b);
    };
    auto patch32 = [&](std::size_t at, std::int32_t v) {
        std::uint32_t u = static_cast<std::uint32_t>(v);
        code_[at]   =  u        & 0xFF;
        code_[at+1] = (u >>  8) & 0xFF;
        code_[at+2] = (u >> 16) & 0xFF;
        code_[at+3] = (u >> 24) & 0xFF;
    };

    // byte-level helpers for common addressing patterns
    // movsx eax, byte [rbp]                : 0F BE 45 00
    auto load_eax_cell  = [&]{ emit({0x0F, 0xBE, 0x45, 0x00}); };
    // mov   [rbp], al                       : 88 45 00
    auto store_al_cell  = [&]{ emit({0x88, 0x45, 0x00}); };

    // Branchless balanced-ternary wrap of eax (assumes |eax| <= MAX + MOD =
    // 364, which holds after +k or x3).  Exactly one +-243 correction (or none)
    // is ever needed, so precompute both candidates and select with cmov   no
    // data-dependent branch to mispredict in tight * loops.  ecx/edx are scratch
    // (caller-saved, unused by our ABI).  lea reads rax: writing eax zero-
    // extends rax, but the low 32 bits of (eax +- 243) are correct regardless.
    //   lea  ecx, [rax-243]   ; 8D 88 0D FF FF FF   ecx = eax-243
    //   lea  edx, [rax+243]   ; 8D 90 F3 00 00 00   edx = eax+243
    //   cmp  eax, 121         ; 83 F8 79
    //   cmovg eax, ecx        ; 0F 4F C1            eax>121  -> eax-243
    //   cmp  eax, -121        ; 83 F8 87
    //   cmovl eax, edx        ; 0F 4C C2            eax<-121 -> eax+243
    // Candidates use the original eax; a fired cmovg lands eax in [-121,121],
    // so the cmovl below never also fires (corrections stay mutually exclusive).
    auto wrap_eax = [&]{
        emit({0x83, 0xF8, 0x79});
        emit({0x7E, 0x05});
        emit({0x2D, 0xF3, 0x00, 0x00, 0x00});
        emit({0x83, 0xF8, 0x87});
        emit({0x7D, 0x05});
        emit({0x05, 0xF3, 0x00, 0x00, 0x00});
    };

    // mov rax, imm64 ; call rax       : 48 B8 imm64 ; FF D0
    auto call_abs = [&](void* fn) {
        emit({0x48, 0xB8});
        emit_w64(reinterpret_cast<std::uint64_t>(fn));
        emit({0xFF, 0xD0});
    };

    // Cell register-cache.  Within a basic block the current cell lives in eax
    // (sign-extended; al = its byte).  Load lazily, defer the store, and flush
    // only at boundaries that move the cell pointer or observe memory: MOVE,
    // JZ/JNZ (branch edges), PUTC (the call clobbers rax), and the epilogue.
    // This collapses a run of cell ops (+ * /) to one load + one store instead
    // of a load+store per op: the structural win over the interpreter.
    bool cell_loaded = false;   // eax holds the current cell value
    bool cell_dirty  = false;   // eax differs from memory, store pending
    auto ensure_loaded = [&]{
        if (!cell_loaded) { load_eax_cell(); cell_loaded = true; cell_dirty = false; }
    };
    auto flush = [&]{
        if (cell_loaded && cell_dirty) { store_al_cell(); cell_dirty = false; }
    };
    auto invalidate = [&]{ cell_loaded = false; cell_dirty = false; };

    // prologue / epilogue
    //   push rbx                 53
    //   push rbp                 55
    //   movabs rbx, &lut         48 BB <imm64>     (rbx = DIV3 LUT base)
    //   mov rbp, rdi             48 89 FD          (rbp = tape base; p=0)
    emit({0x53, 0x55, 0x48, 0xBB});
    emit_w64(reinterpret_cast<std::uint64_t>(btf_div3_lut.data()));
    emit({0x48, 0x89, 0xFD});

    // Bracket-matching state for JZ/JNZ patching.
    std::vector<std::size_t> bracket_jz_patch;

    for (const Op& op : ops) {
        // Debug guard: catch a per-op-budget regression cheaply.  The 32 in
        // the up-front cap was empirically derived; if any op exceeds it
        // this assert trips before we walk off the buffer.
        [[maybe_unused]] std::size_t pos_before = code_pos_;
        switch (op.kind) {
            case Op::ADD: {
                // Pre-wrap k mod 243 (balanced) so |k| <= 121 ==> imm8 fits.
                std::int32_t k = static_cast<std::int32_t>(Cell::wrap(op.arg));
                if (k == 0) break;
                ensure_loaded();
                emit({0x83, 0xC0, static_cast<std::uint8_t>(k)});  // add eax, imm8
                wrap_eax();
                cell_dirty = true;
                break;
            }
            case Op::MOVE: {
                std::int32_t k = op.arg;
                if (k == 0) break;
                flush();                       // cell pointer about to change
                if (k >= -128 && k <= 127) {
                    emit({0x48, 0x83, 0xC5, static_cast<std::uint8_t>(k)}); // add rbp, imm8
                } else {
                    emit({0x48, 0x81, 0xC5});                                // add rbp, imm32
                    emit_w32(static_cast<std::uint32_t>(k));
                }
                invalidate();
                break;
            }
            case Op::MUL3: {
                ensure_loaded();
                emit({0x8D, 0x04, 0x40});      // lea eax, [eax + eax*2]
                wrap_eax();
                cell_dirty = true;
                break;
            }
            case Op::DIV3: {
                // LUT keyed by the cell's raw byte (rbx = LUT base).  Value is
                // already in al; zero-extend it as the index, look up
                // trunc(cell/3), then re-sign-extend so eax keeps the
                // "sign-extended cell" invariant for the next op.
                //   movzx ecx, al          ; 0F B6 C8   (uint8 index)
                //   mov   al,  [rbx + rcx]  ; 8A 04 0B
                //   movsx eax, al           ; 0F BE C0
                ensure_loaded();
                emit({0x0F, 0xB6, 0xC8});
                emit({0x8A, 0x04, 0x0B});
                emit({0x0F, 0xBE, 0xC0});
                cell_dirty = true;
                break;
            }
            case Op::SET: {
                std::int32_t k = static_cast<std::int32_t>(Cell::wrap(op.arg));
                emit({0xB8});                              // mov eax, imm32
                emit_w32(static_cast<std::uint32_t>(k));   // k in [-121,121]
                cell_loaded = true;
                cell_dirty  = true;
                break;
            }
            case Op::PUTC: {
                if (cell_loaded) {
                    flush();                              // keep memory consistent
                    emit({0x0F, 0xBE, 0xF8});             // movsx edi, al
                } else {
                    emit({0x0F, 0xBE, 0x7D, 0x00});       // movsx edi, byte [rbp]
                }
                call_abs(reinterpret_cast<void*>(&btf_jit_putc));
                invalidate();                              // call clobbers rax
                break;
            }
            case Op::JZ: {
                flush(); invalidate();                     // branch edge
                emit({0x80, 0x7D, 0x00, 0x00});           // cmp byte [rbp], 0
                emit({0x0F, 0x84});                       // je rel32 (placeholder)
                std::size_t patch_at = code_pos_;
                emit_w32(0);
                bracket_jz_patch.push_back(patch_at);
                break;
            }
            case Op::JNZ: {
                if (bracket_jz_patch.empty())
                    throw std::runtime_error("BtfJit: unmatched ]");
                std::size_t je_patch = bracket_jz_patch.back();
                bracket_jz_patch.pop_back();
                flush(); invalidate();                     // branch edge
                emit({0x80, 0x7D, 0x00, 0x00});           // cmp byte [rbp], 0
                emit({0x0F, 0x85});                       // jne rel32
                std::size_t body_start = je_patch + 4;
                std::int32_t back = static_cast<std::int32_t>(body_start)
                                  - static_cast<std::int32_t>(code_pos_ + 4);
                emit_w32(static_cast<std::uint32_t>(back));
                // Patch the matching JZ's je: target = end of this JNZ.
                std::int32_t fwd = static_cast<std::int32_t>(code_pos_)
                                 - static_cast<std::int32_t>(je_patch + 4);
                patch32(je_patch, fwd);
                break;
            }
            // SCAN/SIGN/PUTTR/GETTR/GETC + MOVE_ADD/SUB/ADD_AT/SUB_AT all
            // gated out by can_compile(); unreachable here.
            default: __builtin_unreachable();
        }
        assert(code_pos_ - pos_before <= 32 && "JIT op exceeded per-op budget");
        assert(code_pos_ < code_cap_ && "JIT walked off code buffer");
    }

    if (!bracket_jz_patch.empty())
        throw std::runtime_error("BtfJit: unmatched [");

    flush();   // commit the last cell value before returning

    // epilogue: pop rbp ; pop rbx ; ret
    emit({0x5D, 0x5B, 0xC3});

    // The mapping is exactly page-sized to the IR, so flip it all to RX.
    if (::mprotect(code_, code_cap_, PROT_READ | PROT_EXEC) != 0)
        throw std::runtime_error("BtfJit: mprotect rx failed");

    return reinterpret_cast<JitFn>(code_);
}
