// txt2btf: text -> btf source. Emits a program that prints the input verbatim.
//
// Per char, the generator picks the cheaper of:
//   (a) a unary diff on the running cell:  '+'*(v-prev) / '-'*(prev-v) then '.'
//   (b) an absolute Horner build in a fresh cell:  '>' then build(v) then '.'
// Balanced ternary makes (b) cheap (<=5 trits via the *3 op), while (a) wins
// for small steps between neighbouring characters.
//
// One frequent character can also be parked in the constant register: build it
// once, '^' to store, then each occurrence is a single '~'. This both shrinks
// repeats and keeps that char out of the running cell, so its neighbours don't
// have to jump to/from it (e.g. spaces sitting far below a run of letters). The
// best register char (if any) is chosen by trying each and keeping the shortest.
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

// Emit `text`, parking byte `reg` in the constant register (reg < 0 = none).
static std::string gen(const std::string& text, int reg) {
    std::string prog;
    int prev = 0;
    bool started = false;

    if (reg >= 0) {                 // build the register char once, store it
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
        } else {
            int d = v - prev;
            std::string unary = std::string(d >= 0 ? d : -d, d >= 0 ? '+' : '-') + '.';
            std::string fresh = '>' + build(v) + '.';
            prog += unary.size() <= fresh.size() ? unary : fresh;
        }
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

    std::string best = gen(text, -1);            // baseline: no register
    bool seen[256] = {};
    for (char ch : text) {
        int c = static_cast<unsigned char>(ch);
        if (seen[c]) continue;                   // each distinct char once
        seen[c] = true;
        std::string cand = gen(text, c);
        if (cand.size() < best.size()) best = cand;
    }

    // Wrap to keep lines readable; whitespace is ignored by the interpreter.
    constexpr std::size_t WIDTH = 72;
    for (std::size_t i = 0; i < best.size(); i += WIDTH)
        std::cout << best.substr(i, WIDTH) << '\n';
    return 0;
}
