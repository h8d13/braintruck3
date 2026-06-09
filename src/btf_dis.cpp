// btf-dis: annotate a btf program op by op.  A reading aid for generated or
// hand-written code: it traces the cell value through straight-line regions and
// names each print, so `+(()(.` reads as "build 65, print 'A'".  Concrete value
// tracing stops at the first loop or input (iteration/byte unknown); from there
// ops are described structurally.
//
//   btf-dis examples/cafe.btf
//   echo '+(()(.' | btf-dis

#include "reach.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

namespace {

// Render a byte as a readable token: printable ASCII as 'c', else decimal.
std::string show_byte(int b) {
    b &= 0xFF;
    if (b >= 32 && b <= 126) return std::string("'") + static_cast<char>(b) + "'";
    if (b == 10) return "'\\n'";
    if (b == 9)  return "'\\t'";
    return std::to_string(b) + " (0x" + "0123456789abcdef"[(b >> 4) & 0xF] +
           std::string(1, "0123456789abcdef"[b & 0xF]) + ")";
}

int to_byte(int v)     { return v < 0 ? v + 256 : v; }       // '.' two's-complement
int to_byte_low(int v) { return v < 0 ? v + btf::MOD : v; }  // '!' residue

struct Tracer {
    std::optional<int> cur = 0;                 // value of cell at p
    std::map<int, std::optional<int>> saved;    // other cells
    std::optional<int> reg, anc;
    int p = 0;
    bool lost = false;                          // past a loop/input: values unknown

    void drop() { lost = true; cur = std::nullopt; saved.clear(); }
    std::string val(std::optional<int> v) { return v ? std::to_string(*v) : "?"; }

    void move(int d) {
        if (!lost) saved[p] = cur;
        p += d;
        cur = lost ? std::nullopt
                   : (saved.count(p) ? saved[p] : std::optional<int>(0));
    }
};

}  // namespace

int main(int argc, char** argv) {
    std::string raw;
    if (argc > 1 && std::string(argv[1]) != "-") {
        std::ifstream f(argv[1]);
        if (!f) { std::cerr << "cannot open " << argv[1] << '\n'; return 1; }
        raw.assign(std::istreambuf_iterator<char>(f), {});
    } else {
        std::ostringstream ss; ss << std::cin.rdbuf(); raw = ss.str();
    }

    Tracer t;
    bool in_comment = false;
    auto line = [&](const std::string& src, const std::string& note) {
        std::cout << "  " << src;
        for (std::size_t k = src.size(); k < 8; ++k) std::cout << ' ';
        std::cout << note << '\n';
    };

    for (std::size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (in_comment) { if (c == '\n') in_comment = false; continue; }
        switch (c) {
            case '#': in_comment = true; break;
            case '+': case '-': {
                int n = 0; std::size_t j = i;
                while (j < raw.size() && (raw[j] == '+' || raw[j] == '-'))
                    n += (raw[j++] == '+') ? 1 : -1;
                std::string src(raw, i, j - i); i = j - 1;
                if (t.cur) t.cur = btf::wrap(*t.cur + n);
                line(src, "cell += " + std::to_string(n) +
                          (t.cur ? "  -> " + t.val(t.cur) : ""));
                break;
            }
            case '>': case '<': {
                int d = 0; std::size_t j = i;
                while (j < raw.size() && (raw[j] == '>' || raw[j] == '<'))
                    d += (raw[j++] == '>') ? 1 : -1;
                std::string src(raw, i, j - i); i = j - 1;
                t.move(d);
                line(src, "move p " + std::string(d >= 0 ? "+" : "") +
                          std::to_string(d) + "  -> cell[" + std::to_string(t.p) +
                          "] = " + t.val(t.cur));
                break;
            }
            case '*': if (t.cur) t.cur = btf::wrap(*t.cur * 3);
                      line("*", "cell *= 3  -> " + t.val(t.cur)); break;
            case '/': if (t.cur) t.cur = *t.cur / 3;
                      line("/", "cell /= 3  -> " + t.val(t.cur)); break;
            case '(': if (t.cur) t.cur = btf::wrap(*t.cur * 3 + 1);
                      line("(", "cell = cell*3+1  -> " + t.val(t.cur)); break;
            case ')': if (t.cur) t.cur = btf::wrap(*t.cur * 3 - 1);
                      line(")", "cell = cell*3-1  -> " + t.val(t.cur)); break;
            case '?': if (t.cur) t.cur = (*t.cur > 0) - (*t.cur < 0);
                      line("?", "cell = sign(cell)  -> " + t.val(t.cur)); break;
            case '.': line(".", t.cur ? "print " + show_byte(to_byte(*t.cur))
                                      : "print cell as byte"); break;
            case '!': line("!", t.cur ? "print " + show_byte(to_byte_low(*t.cur))
                                      : "print cell as residue byte"); break;
            case ',': line(",", "read byte into cell"); t.cur = std::nullopt; break;
            case ':': line(":", "print cell as trits"); break;
            case ';': line(";", "read trits into cell"); t.cur = std::nullopt; break;
            case '^': t.reg = t.cur; line("^", "register = cell (" + t.val(t.cur) + ")"); break;
            case '~': line("~", t.reg ? "print register " + show_byte(to_byte(*t.reg))
                                      : "print register"); break;
            case '@': t.anc = t.cur; line("@", "anchor = cell (" + t.val(t.cur) + ")"); break;
            case '_': t.cur = t.anc; line("_", "cell = anchor  -> " + t.val(t.cur)); break;
            case '[': line("[", "while cell != 0:"); t.drop(); break;
            case ']': line("]", "end loop"); break;
            case 'U':
                t.cur = t.anc;
                line("U", "cell = anchor, print " +
                          (t.cur ? show_byte(to_byte(*t.cur)) : "cell"));
                break;
            default: {
                static const char* STEP[6] = {"+1", "-1", "*3", "/3", "*3+1", "*3-1"};
                std::string g(1, c);
                std::size_t k;
                if ((k = std::string(btf::FUSED_PRINT).find(c)) != std::string::npos) {
                    if (t.cur) t.cur = btf::apply_step(*t.cur, static_cast<int>(k));
                    line(g, "cell = cell" + std::string(STEP[k]) + ", print " +
                            (t.cur ? show_byte(to_byte(*t.cur)) : "cell"));
                } else if ((k = std::string(btf::ANCREL_PRINT).find(c)) != std::string::npos) {
                    t.cur = t.anc ? std::optional<int>(
                                btf::apply_step(*t.anc, static_cast<int>(k)))
                                  : std::nullopt;
                    line(g, "cell = anchor" + std::string(STEP[k]) + ", print " +
                            (t.cur ? show_byte(to_byte(*t.cur)) : "cell"));
                } else if ((k = std::string(btf::BUILD_STORE).find(c)) != std::string::npos) {
                    if (t.cur) t.cur = btf::apply_step(*t.cur, static_cast<int>(k));
                    t.anc = t.cur;
                    line(g, "cell = cell" + std::string(STEP[k]) +
                            ", anchor = cell (" + t.val(t.cur) + ")");
                }
                break;  // whitespace / unknown
            }
        }
    }
    return 0;
}
