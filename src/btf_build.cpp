// btf-build: print the shortest btf op-string that leaves a given value in the
// current cell.  A hand-authoring aid: "how do I get the cell to 65?" -> ++(()(.
// Exposes the same BFS reach() the translator uses internally.
//
//   btf-build 65            -> ++(()(
//   btf-build -f 32 99      -> +*        (reach 32 -> 99, off the value in cell)
//   btf-build 65 32 10      -> one "VALUE\tOPS" line each
//   btf-build -c A          -> ++(()(.   (build then print the char, '.' or '!')
//
// Values are cell targets (any int, wrapped mod 243).  With -c, each arg is text
// and every byte is built then printed with its encode() terminator.

#include "reach.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void usage(const char* prog) {
    std::cerr << "usage: " << prog << " [-f FROM] [-c] VALUE...\n"
              << "  VALUE   target cell value (int), or text if -c\n"
              << "  -f FROM start cell value (default 0)\n"
              << "  -c      VALUE is text: build each byte and append its print op\n";
}
}  // namespace

int main(int argc, char** argv) {
    int from = 0;
    bool as_text = false;
    int i = 1;
    for (; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-c") as_text = true;
        else if (a == "-f" && i + 1 < argc) from = std::atoi(argv[++i]);
        else if (!a.empty() && a[0] == '-' && a.size() > 1 &&
                 !(a[1] >= '0' && a[1] <= '9')) { usage(argv[0]); return 1; }
        else break;  // first non-flag: start of VALUE list
    }
    if (i >= argc) { usage(argv[0]); return 1; }

    bool multi = (argc - i) > 1 || as_text;
    for (; i < argc; ++i) {
        if (as_text) {
            for (unsigned char ch : std::string(argv[i])) {
                btf::Enc e = btf::encode(ch);
                std::string ops = btf::reach(from, e.target) + e.term;
                std::cout << static_cast<char>(ch) << '\t' << ops << '\n';
            }
        } else {
            int v = std::atoi(argv[i]);
            std::string ops = btf::reach(from, v);
            if (multi) std::cout << v << '\t' << ops << '\n';
            else       std::cout << ops << '\n';
        }
    }
    return 0;
}
