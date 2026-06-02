#pragma once

#include <cstdint>

// IR op shared between the interpreter (btf.cpp) and the JIT (jit.cpp).
// Order MUST match the dispatch table in btf.cpp's run().

struct Op {
    enum Kind : std::uint8_t {
        ADD, MOVE,
        MUL3, DIV3, SIGN,
        PUTC, GETC, PUTTR, GETTR,
        JZ, JNZ,
        SET,        // SET k     : cell = k
        MOVE_ADD,   // MOVE_ADD d: tape[p+d] += cell; cell = 0
        MOVE_SUB,   // MOVE_SUB d: tape[p+d] -= cell; cell = 0
        ADD_AT,     // ADD_AT  d : tape[p+d] += cell
        SUB_AT,     // SUB_AT  d : tape[p+d] -= cell
        SCAN        // SCAN    d : while (!tape[p].zero) p += d
    } kind;
    std::int32_t arg;
};
