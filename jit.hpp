#pragma once

#include "ir.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>

// x86_64 Linux JIT for btf IR.  Emits raw machine code into an mmap'd region
// and toggles W^X via mprotect.  No external dependencies.
//
// ABI of compiled function:  void f(std::int8_t* tape)
//   rdi  = tape base on entry
//   rbx  = tape base (callee-saved copy)
//   rbp  = current cell pointer (= tape + p)  (callee-saved)
//
// Supported ops only: ADD, MOVE, MUL3, DIV3, SET, JZ/JNZ, MOVE_ADD/SUB,
// ADD_AT/SUB_AT, PUTC, GETC.  SCAN/SIGN/PUTTR/GETTR fall back to the
// interpreter (see can_compile).  All cell ops do balanced-ternary wrap
// inline; PUTC/GETC call C runtime helpers via abs64.

class BtfJit {
public:
    BtfJit();
    ~BtfJit();
    BtfJit(const BtfJit&) = delete;
    BtfJit& operator=(const BtfJit&) = delete;

    using JitFn = void (*)(std::int8_t* tape);
    JitFn compile(const std::vector<Op>& ops);

    // True if the IR uses only ops the JIT supports.  SCAN, SIGN, and the
    // trit-string I/O ops fall back to the interpreter (interp's memchr-based
    // scan beats inline codegen 50-100x; the others are rare and complex).
    static bool can_compile(const std::vector<Op>& ops);

private:
    std::uint8_t* code_;
    std::size_t   code_cap_;
    std::size_t   code_pos_;
};
