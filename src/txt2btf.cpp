// txt2btf: text -> btf source. Emits a program that prints the input verbatim.
//
// Strategy (same as examples/arch.btf): cell0 = writer holding the last char,
// cell1 = scratch. Each char's value is reached by a diff from the previous;
// the diff's magnitude is Horner-built in scratch from balanced trits, then
// drained into the writer via [<+>-] (add) or [<->-] (subtract).
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

// Horner build: '*' (x3, skipped on first trit) then '+'/'-'/nothing per trit.
static std::string build(int magnitude) {
    std::string s;
    bool first = true;
    for (int t : trits(magnitude)) {
        if (!first) s += '*';
        first = false;
        if (t == 1) s += '+';
        else if (t == -1) s += '-';
    }
    return s;
}

// Ops to move the writer from `from` to `to` (scratch assumed 0, left at 0).
// A fresh writer (from == 0) builds the value directly; otherwise the diff's
// magnitude is built in scratch and drained in (+ when to > from, - when below).
static std::string move(int from, int to) {
    if (from == 0) return build(to);
    int diff = to - from;
    if (diff == 0) return "";
    return '>' + build(diff) + (diff > 0 ? "[<+>-]" : "[<->-]") + "<";
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

    std::string prog;
    int prev = 0;
    for (char ch : text) {
        int v = static_cast<unsigned char>(ch);
        prog += move(prev, v) + '.';
        prev = v;
    }

    // Wrap to keep lines readable; whitespace is ignored by the interpreter.
    constexpr std::size_t WIDTH = 72;
    for (std::size_t i = 0; i < prog.size(); i += WIDTH)
        std::cout << prog.substr(i, WIDTH) << '\n';
    return 0;
}
