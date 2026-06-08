// txt2btf: text -> btf source. Emits a program that prints the input verbatim.
//
// Per char, the generator picks the cheaper of:
//   (a) a unary diff on the running cell:  '+'*(v-prev) / '-'*(prev-v) then '.'
//   (b) an absolute Horner build in a fresh cell:  '>' then build(v) then '.'
// Balanced ternary makes (b) cheap (<=5 trits via the *3 op), while (a) wins
// for small steps between neighbouring characters.
//
// Two registers help further:
//   - constant register (^/~): park a frequent char, emit each occurrence as a
//     single '~'. Keeps it out of the running cell, so neighbours don't jump
//     to/from it (e.g. spaces far below a run of letters).
//   - anchor register (@/_): park a build base; chars near it become '_' + a
//     small diff instead of a full Horner build (a tight cluster of letters).
// The best (register, anchor) pair is chosen by trying every combination over
// the distinct characters and keeping the shortest program.
//
// Text comes from argv (joined with spaces) or stdin if no args.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Balanced-ternary trits of |n|, most-significant first.
static std::vector<int> trits(int n) {
    n = n < 0 ? -n : n;
    if (n == 0) return {0};
    std::vector<int> ds;
    while (n) {
        int r = n % 3;
        n /= 3;
        if (r == 0)      ds.push_back(0);
        else if (r == 1) ds.push_back(1);
        else           { ds.push_back(-1); ++n; }  // carry: 2 == 3 + (-1)
    }
    return {ds.rbegin(), ds.rend()};
}

// Horner build of a value into a zero cell: '*' (x3, skipped on the first trit)
// then '+'/'-'/nothing per trit, MSB to LSB.
static std::string build(int v) {
    std::string s;
    bool first = true;
    for (int t : trits(v)) {
        if (!first) s += '*';
        first = false;
        if (t == 1) s += '+';
        else if (t == -1) s += '-';
    }
    return s;
}

// Unary run of `n` '+' or '-', then a '.'  ("" run when n==0, so just '.').
static std::string unary(int n) {
    return std::string(n >= 0 ? n : -n, n >= 0 ? '+' : '-') + '.';
}

// Emit `text`, optionally parking one byte in the constant register (reg, for
// cheap reprints via ~) and one in the anchor register (anc, a build base
// recalled via _ then nudged by a small diff).  reg/anc < 0 = unused.
static std::string gen(const std::string& text, int reg, int anc) {
    std::string prog;
    int prev = 0;
    bool started = false;

    if (anc >= 0) {                 // build the anchor base once, store it
        prog += build(anc);
        prog += '@';
        prev = anc;
        started = true;
    }
    if (reg >= 0) {                 // build the register char once, store it
        if (started) prog += '>';   // on a fresh cell, leaving the anchor cell
        prog += build(reg);
        prog += '^';
        prev = reg;                 // the build cell becomes the running cell
        started = true;
    }

    for (char ch : text) {
        int v = static_cast<unsigned char>(ch);
        if (reg >= 0 && v == reg) { prog += '~'; continue; }
        if (!started) {             // first char built directly into cell0 (==0)
            prog += build(v) + '.';
            started = true;
            prev = v;
            continue;
        }
        std::string best = unary(v - prev);          // diff on the running cell
        std::string fresh = '>' + build(v) + '.';    // absolute build, fresh cell
        if (fresh.size() < best.size()) best = fresh;
        if (anc >= 0) {                               // recall anchor + small diff
            std::string rec = '_' + unary(v - anc);
            if (rec.size() < best.size()) best = rec;
        }
        prog += best;
        prev = v;
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
    // for each), keep the shortest. Each is a candidate base/frequent char.
    std::vector<int> cands = {-1};
    bool seen[256] = {};
    for (char ch : text) {
        int c = static_cast<unsigned char>(ch);
        if (!seen[c]) { seen[c] = true; cands.push_back(c); }
    }

    std::string best = gen(text, -1, -1);
    for (int reg : cands)
        for (int anc : cands) {
            std::string cand = gen(text, reg, anc);
            if (cand.size() < best.size()) best = cand;
        }

    // Wrap to keep lines readable; whitespace is ignored by the interpreter.
    constexpr std::size_t WIDTH = 72;
    for (std::size_t i = 0; i < best.size(); i += WIDTH)
        std::cout << best.substr(i, WIDTH) << '\n';
    return 0;
}
