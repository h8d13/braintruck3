#include "jit.hpp"
#include "cell.hpp"

#include <sys/mman.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <cstdlib>

// ===== Runtime helpers callable from JIT'd code (extern "C" so the symbol
// is reachable via raw absolute call from emitted machine code). ============

extern "C" void btf_jit_putc(std::int8_t c) {
    std::cout.put(static_cast<char>(static_cast<unsigned char>(c)));
}
extern "C" std::int8_t btf_jit_getc() {
    int ch = std::cin.get();
    return Cell(ch == EOF ? 0 : ch).value();
}

// ===== JIT impl ============================================================

namespace {
constexpr std::size_t CODE_CAP = 16 * 1024 * 1024;  // 16 MB
}

BtfJit::BtfJit() : code_(nullptr), code_cap_(CODE_CAP), code_pos_(0) {
    code_ = static_cast<std::uint8_t*>(::mmap(
        nullptr, code_cap_,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0));
    if (code_ == MAP_FAILED) {
        code_ = nullptr;
        throw std::runtime_error("BtfJit: mmap failed");
    }
}

BtfJit::~BtfJit() {
    if (code_) ::munmap(code_, code_cap_);
}

bool BtfJit::can_compile(const std::vector<Op>& ops) {
    // Two requirements:
    //   1. No unsupported ops (SCAN/SIGN/trit-I/O   interp wins on these).
    //   2. At least one MUL3 or DIV3   these are the ternary primitives where
    //      JIT actually pays off.  Without them the program is BF-equivalent
    //      and our interp matches or beats JIT (no codegen amortization).
    bool has_arith = false;
    for (const Op& op : ops) {
        switch (op.kind) {
            case Op::SCAN: case Op::SIGN:
            case Op::PUTTR: case Op::GETTR:
                return false;
            case Op::MUL3: case Op::DIV3:
                has_arith = true;
                break;
            default: break;
        }
    }
    return has_arith;
}

BtfJit::JitFn BtfJit::compile(const std::vector<Op>& ops) {
    code_pos_ = 0;
    if (::mprotect(code_, code_cap_, PROT_READ | PROT_WRITE) != 0) {
        throw std::runtime_error("BtfJit: mprotect rw failed");
    }

    // Bounds-check up front: the worst-case per-op size is ~64 bytes; we
    // already reserved 16 MB.  Skip per-byte cap checks in the hot path.
    if (ops.size() * 80 + 256 > code_cap_)
        throw std::runtime_error("BtfJit: IR too large for code buffer");
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

    // ---- byte-level helpers for common addressing patterns -----------------
    // movsx eax, byte [rbp]                : 0F BE 45 00
    auto load_eax_cell  = [&]{ emit({0x0F, 0xBE, 0x45, 0x00}); };
    // mov   [rbp], al                       : 88 45 00
    auto store_al_cell  = [&]{ emit({0x88, 0x45, 0x00}); };
    // mov   byte [rbp], imm8                : C6 45 00 imm8
    auto store_imm_cell = [&](std::int8_t v) {
        emit({0xC6, 0x45, 0x00, static_cast<std::uint8_t>(v)});
    };

    // movsx <reg>, byte [rbp + d]   reg encoded in ModR/M
    //   reg=0 (eax): 0F BE 45/85
    //   reg=1 (ecx): 0F BE 4D/8D
    //   reg=7 (edi): 0F BE 7D/BD
    auto load_reg_off = [&](std::uint8_t reg, std::int32_t d) {
        emit({0x0F, 0xBE});
        if (d >= -128 && d <= 127) {
            emit_b(0x40 | (reg << 3) | 0x05);                  // mod=01, r/m=5
            emit_b(static_cast<std::uint8_t>(d));
        } else {
            emit_b(0x80 | (reg << 3) | 0x05);                  // mod=10, r/m=5
            emit_w32(static_cast<std::uint32_t>(d));
        }
    };
    // mov [rbp+d], al                : 88 (mod r/m for [rbp+disp]) ; src reg = 0
    auto store_al_off = [&](std::int32_t d) {
        emit_b(0x88);
        if (d >= -128 && d <= 127) {
            emit_b(0x45);
            emit_b(static_cast<std::uint8_t>(d));
        } else {
            emit_b(0x85);
            emit_w32(static_cast<std::uint32_t>(d));
        }
    };

    // Branchful balanced-ternary wrap of eax (assumes |eax| <= MAX + MOD).
    //   cmp eax, 121         ; 83 F8 79
    //   jle .neg_check       ; 7E 05
    //   sub eax, 243         ; 2D F3 00 00 00
    //   .neg_check:
    //   cmp eax, -121        ; 83 F8 87
    //   jge .done            ; 7D 05
    //   add eax, 243         ; 05 F3 00 00 00
    //   .done:
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

    // ---- prologue / epilogue ----------------------------------------------
    //   push rbx           53
    //   push rbp           55
    //   mov rbx, rdi       48 89 FB
    //   mov rbp, rdi       48 89 FD     (rbp = tape base; p starts at 0)
    emit({0x53, 0x55, 0x48, 0x89, 0xFB, 0x48, 0x89, 0xFD});

    // Bracket-matching state for JZ/JNZ patching.
    std::vector<std::size_t> bracket_jz_patch;

    for (const Op& op : ops) {
        switch (op.kind) {
            case Op::ADD: {
                // Pre-wrap k mod 243 (balanced) so |k| <= 121 ==> imm8 fits.
                std::int32_t k = static_cast<std::int32_t>(Cell::wrap(op.arg));
                if (k == 0) break;
                load_eax_cell();
                emit({0x83, 0xC0, static_cast<std::uint8_t>(k)});  // add eax, imm8
                wrap_eax();
                store_al_cell();
                break;
            }
            case Op::MOVE: {
                std::int32_t k = op.arg;
                if (k == 0) break;
                if (k >= -128 && k <= 127) {
                    emit({0x48, 0x83, 0xC5, static_cast<std::uint8_t>(k)}); // add rbp, imm8
                } else {
                    emit({0x48, 0x81, 0xC5});                                // add rbp, imm32
                    emit_w32(static_cast<std::uint32_t>(k));
                }
                break;
            }
            case Op::MUL3: {
                load_eax_cell();
                emit({0x8D, 0x04, 0x40});      // lea eax, [eax + eax*2]
                wrap_eax();
                store_al_cell();
                break;
            }
            case Op::DIV3: {
                load_eax_cell();
                emit({0xB9, 0x03, 0x00, 0x00, 0x00});  // mov ecx, 3
                emit_b(0x99);                            // cdq
                emit({0xF7, 0xF9});                       // idiv ecx
                // Result in eax, range fits cell; no wrap needed.
                store_al_cell();
                break;
            }
            case Op::SET: {
                std::int32_t k = static_cast<std::int32_t>(Cell::wrap(op.arg));
                store_imm_cell(static_cast<std::int8_t>(k));
                break;
            }
            case Op::PUTC: {
                emit({0x0F, 0xBE, 0x7D, 0x00});           // movsx edi, byte [rbp]
                call_abs(reinterpret_cast<void*>(&btf_jit_putc));
                break;
            }
            case Op::GETC: {
                call_abs(reinterpret_cast<void*>(&btf_jit_getc));
                store_al_cell();                          // helper returned int8 in al
                break;
            }
            case Op::JZ: {
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
            case Op::MOVE_ADD: {
                // tape[p+d] += cell; cell = 0
                std::int32_t d = op.arg;
                load_eax_cell();                          // eax = cell
                load_reg_off(1, d);                       // ecx = tape[p+d]
                emit({0x01, 0xC1});                       // add ecx, eax
                emit({0x89, 0xC8});                       // mov eax, ecx
                wrap_eax();
                store_al_off(d);
                store_imm_cell(0);
                break;
            }
            case Op::MOVE_SUB: {
                std::int32_t d = op.arg;
                load_reg_off(1, d);                       // ecx = tape[p+d]
                load_eax_cell();                          // eax = cell
                emit({0x29, 0xC1});                       // sub ecx, eax
                emit({0x89, 0xC8});                       // mov eax, ecx
                wrap_eax();
                store_al_off(d);
                store_imm_cell(0);
                break;
            }
            case Op::ADD_AT: {
                std::int32_t d = op.arg;
                load_eax_cell();
                load_reg_off(1, d);
                emit({0x01, 0xC1});
                emit({0x89, 0xC8});
                wrap_eax();
                store_al_off(d);
                break;
            }
            case Op::SUB_AT: {
                std::int32_t d = op.arg;
                load_reg_off(1, d);
                load_eax_cell();
                emit({0x29, 0xC1});
                emit({0x89, 0xC8});
                wrap_eax();
                store_al_off(d);
                break;
            }
            // SIGN/PUTTR/GETTR/SCAN gated by can_compile(); unreachable.
            default: __builtin_unreachable();
        }
    }

    if (!bracket_jz_patch.empty())
        throw std::runtime_error("BtfJit: unmatched [");

    // epilogue: pop rbp ; pop rbx ; ret
    emit({0x5D, 0x5B, 0xC3});

    if (::mprotect(code_, code_cap_, PROT_READ | PROT_EXEC) != 0)
        throw std::runtime_error("BtfJit: mprotect rx failed");

    return reinterpret_cast<JitFn>(code_);
}
