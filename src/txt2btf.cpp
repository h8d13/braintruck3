// txt2btf: text -> btf source. Emits a program that prints the input verbatim.
//
// Per char, the generator reaches the target value by the cheapest of:
//   (a) affine ops on the running cell:  reach(prev, v) then '.'
//   (b) an absolute build in a fresh cell:  '>' then reach(0, v) then '.'
//   (c) anchor recall + affine:  '_' then reach(anchor, v) then '.'
// reach() is a BFS over the ops the cell already supports (+1, -1, x3, /3), so
// it finds the shortest path whether that's a unary walk, a from-zero Horner
// build, or a multiply off the value already in the cell (e.g. (32+1)*3 == 99,
// '+*', far cheaper than rebuilding a letter from scratch).
//
// Two registers help further:
//   - constant register (^/~): park a frequent char, emit each occurrence as a
//     single '~'. Keeps it out of the running cell, so neighbours don't jump
//     to/from it (e.g. spaces far below a run of letters).
//   - anchor register (@/_): park a build base; chars near it become '_' + a
//     small reach instead of a full from-zero build (a tight cluster of letters).
// The best (register, anchor) pair is chosen by trying every combination over
// the distinct characters and keeping the shortest program.
//
// Text comes from argv (joined with spaces) or stdin if no args.

#include <array>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int MOD = 243, MAXV = 121;  // cell range -121..121, wrap mod 243

// Match the interpreter: x3/+/- wrap mod 243, /3 truncates toward zero.
int wrap(int x) { return ((x + MAXV) % MOD + MOD) % MOD - MAXV; }

// Shortest op-string taking a cell from `from` to `to` over the cell ops
// {+1, -1, x3, /3} plus the fused Horner steps {x3+1 '(', x3-1 ')'}.  One BFS
// subsumes unary diffs and from-zero builds, finds multiplies off the current
// value, and lands each balanced-ternary trit in a single op via '('/')'.
std::string reach(int from, int to) {
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

// How to print byte b: the cell value to build, and the print op for it.
//   0..121    cell == b,        '.'  (to_byte is the identity here)
//   122..134  cell == b - 243,  '!'  (dead band: residue print, cells -121..-109)
//   135..255  cell == b - 256,  '.'  (high bytes ride the negative cell range)
struct Enc { int target; char term; };
Enc encode(int b) {
    if (b <= 121) return {b, '.'};
    if (b <= 134) return {b - 243, '!'};
    return {b - 256, '.'};
}
}  // namespace

// Emit `text`, optionally parking one byte in the constant register (reg, for
// cheap reprints via ~) and one in the anchor register (anc, a build base
// recalled via _ then nudged by a small diff).  reg/anc < 0 = unused.
// reg/anc are byte values (or <0 = unused). The cell holds signed build values,
// so both park their encoded target; reg must be a '.'-printable byte (its only
// output path is '~' == to_byte), the caller filters the dead band out of it.
static std::string gen(const std::string& text, int reg, int anc) {
    std::string prog;
    int prev = 0;
    bool started = false;
    int anc_base = anc >= 0 ? encode(anc).target : 0;

    if (anc >= 0) {                 // build the anchor base once, store it
        prog += reach(0, anc_base);
        prog += '@';
        prev = anc_base;
        started = true;
    }
    if (reg >= 0) {                 // build the register char once, store it
        if (started) prog += '>';   // on a fresh cell, leaving the anchor cell
        prog += reach(0, encode(reg).target);
        prog += '^';
        prev = encode(reg).target;  // the build cell becomes the running cell
        started = true;
    }

    for (char ch : text) {
        int b = static_cast<unsigned char>(ch);
        if (reg >= 0 && b == reg) { prog += '~'; continue; }
        Enc e = encode(b);
        if (!started) {             // first char built directly into cell0 (==0)
            prog += reach(0, e.target) + e.term;
            started = true;
            prev = e.target;
            continue;
        }
        std::string best = reach(prev, e.target);     // affine on the running cell
        std::string fresh = '>' + reach(0, e.target); // absolute build, fresh cell
        if (fresh.size() < best.size()) best = fresh;
        if (anc >= 0) {                               // recall anchor + reach
            std::string rec = '_' + reach(anc_base, e.target);
            if (rec.size() < best.size()) best = rec;
        }
        prog += best + e.term;
        prev = e.target;
    }
    return prog;
}

int main(int argc, char** argv) {
    std::string text;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (i > 1) text += ' ';
            text += argv[i];
        }
    } else {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        text = ss.str();
    }

    // Search every (register, anchor) pair over the distinct chars (plus "none"
    // for each), keep the shortest. anc is any char (a build base); reg prints
    // via '~' == to_byte, so dead-band bytes (122..134) are excluded from it.
    std::vector<int> anc_cands = {-1};
    std::vector<int> reg_cands = {-1};
    bool seen[256] = {};
    for (char ch : text) {
        int c = static_cast<unsigned char>(ch);
        if (seen[c]) continue;
        seen[c] = true;
        anc_cands.push_back(c);
        if (c < 122 || c > 134) reg_cands.push_back(c);
    }

    std::string best = gen(text, -1, -1);
    for (int reg : reg_cands)
        for (int anc : anc_cands) {
            std::string cand = gen(text, reg, anc);
            if (cand.size() < best.size()) best = cand;
        }

    // Wrap to keep lines readable; whitespace is ignored by the interpreter.
    constexpr std::size_t WIDTH = 72;
    for (std::size_t i = 0; i < best.size(); i += WIDTH)
        std::cout << best.substr(i, WIDTH) << '\n';
    return 0;
}
