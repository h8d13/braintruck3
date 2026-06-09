#pragma once

// Shared value-building primitives for the btf tools (txt2btf, btf-build,
// btf-dis).  Header-only: a BFS shortest-op builder plus the byte<->cell
// encoding, both matching the interpreter's wrap semantics exactly.

#include <array>
#include <queue>
#include <string>
#include <vector>

namespace btf {

constexpr int MOD = 243, MAXV = 121;  // cell range -121..121, wrap mod 243

// Match the interpreter: x3/+/- wrap mod 243, /3 truncates toward zero.
inline int wrap(int x) { return ((x + MAXV) % MOD + MOD) % MOD - MAXV; }

// Shortest op-string taking a cell from `from` to `to` over the cell ops
// {+1, -1, x3, /3} plus the fused Horner steps {x3+1 '(', x3-1 ')'}.  One BFS
// subsumes unary diffs and from-zero builds, finds multiplies off the current
// value, and lands each balanced-ternary trit in a single op via '('/')'.
inline std::string reach(int from, int to) {
    from = wrap(from);
    to   = wrap(to);
    auto idx = [](int v) { return v + MAXV; };
    std::array<bool, MOD> seen{};
    std::vector<std::string> path(MOD);
    std::queue<int> q;
    seen[idx(from)] = true;
    q.push(from);
    while (!q.empty()) {
        int c = q.front();
        q.pop();
        if (c == to) return path[idx(c)];
        const std::pair<int, char> nbr[] = {
            {wrap(c + 1),     '+'}, {wrap(c - 1),     '-'},
            {wrap(c * 3),     '*'}, {c / 3,           '/'},
            {wrap(c * 3 + 1), '('}, {wrap(c * 3 - 1), ')'},
        };
        for (auto [v, op] : nbr) {
            if (!seen[idx(v)]) {
                seen[idx(v)] = true;
                path[idx(v)] = path[idx(c)] + op;
                q.push(v);
            }
        }
    }
    return path[idx(to)];  // unreachable: {+1,x3} alone already span the ring
}

// Fused-op glyph families, shared by the compiler (btf.cpp), disassembler and
// translator. Index k = 0..5 selects the arithmetic step {+1, -1, x3, /3,
// x3+1, x3-1}; the glyph fuses that step with a print or an anchor store, so
// the IR stays unchanged and only source density improves.
constexpr const char* FUSED_PRINT  = "PMTDLR";  // step on cell, then print
constexpr const char* ANCREL_PRINT = "pmtdlr";  // cell = step(anchor), print
constexpr const char* BUILD_STORE  = "&=$%{}";  // step on cell, anchor = cell
// 'U' completes ANCREL_PRINT as the identity case: cell = anchor, print.

inline int apply_step(int v, int k) {
    switch (k) {
        case 0:  return wrap(v + 1);
        case 1:  return wrap(v - 1);
        case 2:  return wrap(v * 3);
        case 3:  return v / 3;
        case 4:  return wrap(v * 3 + 1);
        default: return wrap(v * 3 - 1);
    }
}

// How to print byte b: the cell value to build, and the print op for it.
//   0..121    cell == b,        '.'  (to_byte is the identity here)
//   122..134  cell == b - 243,  '!'  (dead band: residue print, cells -121..-109)
//   135..255  cell == b - 256,  '.'  (high bytes ride the negative cell range)
struct Enc { int target; char term; };
inline Enc encode(int b) {
    if (b <= 121) return {b, '.'};
    if (b <= 134) return {b - 243, '!'};
    return {b - 256, '.'};
}

}  // namespace btf
