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
        SCAN,       // SCAN    d : while (!tape[p].zero) p += d
        // Constant register: print a stored byte cheaply, without disturbing
        // the running cell (e.g. a frequent char like ' ' interleaved in text).
        REG_STORE,  // ^  : R = current cell
        REG_PUT,    // ~  : print R as a byte (cell + pointer untouched)
        // Anchor register: a second slot used as a build base. Recall it into
        // the cell, then a small diff reaches a nearby value, cheaper than a
        // full Horner build for a cluster of close characters.
        ANC_STORE,  // @  : A = current cell
        ANC_RECALL  // _  : current cell = A
    } kind;
    std::int32_t arg;
};
