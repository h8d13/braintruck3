#pragma once

#include <cstdint>

// IR op for the interpreter (btf.cpp).
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
        ANC_RECALL, // _  : current cell = A
        // Fused Horner step: one trit shift plus its sign in a single op, so a
        // balanced-ternary build costs ~one op per trit instead of '*' + '+'/'-'.
        MUL3_INC,   // (  : cell = cell*3 + 1
        MUL3_DEC,   // )  : cell = cell*3 - 1
        // Residue print: emit v mod 243 as a low byte (0..242), reaching the
        // 122..134 band that PUTC's two's-complement byte cast skips over.
        PUTC_RES    // !  : print to_byte_low()
    } kind;
    std::int32_t arg;
};
