// txt2btf: text -> btf source. Emits a program that prints the input verbatim.
//
// Exact search: a layered BFS over interpreter state (chars printed, cell
// value, anchor register, constant register). All ops cost 1, so the first
// path reaching "all chars printed" is a shortest program. This subsumes the
// old per-char greedy: it derives the next char from whatever the cell,
// anchor, or a '/'-byproduct happens to offer, re-stores the constant
// register mid-program when a cheaper park value floats by, and places '@'
// anywhere instead of only at program start.
//
// Modeled ops: cell arithmetic (+ - * / ( )), '>' onto a fresh cell (the
// pointer only ever moves right, so the target cell is always 0), '@'/'_'
// anchor store/recall, '^'/'~' constant store/print, '.'/'!' prints, and the
// fused families (step+print P M T D L R / U p m t d l r, step+store
// & = $ % { }) which collapse a build step and its print or anchor store
// into one glyph. Candidate registers are restricted to values the program
// could actually use: anchors to the encode-targets of the text's chars plus
// their one-step preimage hubs, constants to its printable bytes. '?' and
// loops never pay for themselves on straight text.
//
// Long inputs are split into chunks sized to a state budget; the running
// (cell, anchor, constant) state carries across the boundary, so only the
// chunk seams lose optimality.
//
// Text comes from argv (joined with spaces) or stdin if no args.

#include "reach.hpp"

#include <cstdint>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

using btf::encode;
using btf::wrap;
using btf::MAXV;

namespace {

constexpr int  ANC_NONE   = 999;        // anchor register empty
constexpr long MAX_STATES = 48L << 20;  // per-chunk search budget (~300 MB)

bool dead_band(int b) { return b >= 122 && b <= 134; }  // '~' can't print these

// Anchor candidates for one target: the target itself plus every value one
// arithmetic step away from it. A store can then park a hub that reaches
// several targets via single fused ops (e.g. 113 serves 'r' via p and 'a'
// via l), which target-only candidates would miss.
template <typename F>
void for_anchor_candidates(int target, F&& f) {
    f(target);
    for (int v = -MAXV; v <= MAXV; ++v)
        for (int k = 0; k < 6; ++k)
            if (btf::apply_step(v, k) == target) f(v);
}

// Exact shortest program printing `text` from entry state (cell, anc, con);
// all three are updated to the exit state for the next chunk.
std::string solve(const std::string& text, int& cell, int& anc, int& con) {
    const int N = static_cast<int>(text.size());

    // Candidate sets. Index 0 is always "empty"; the carried-in values must
    // be present to encode the entry state.
    std::vector<int> ancs = {ANC_NONE};
    std::vector<int> cons = {-1};
    bool sa[btf::MOD] = {}, sc[256] = {};
    auto add_anc = [&](int v) {
        if (v == ANC_NONE) return;
        if (!sa[v + MAXV]) { sa[v + MAXV] = true; ancs.push_back(v); }
    };
    auto add_con = [&](int b) {
        if (b < 0 || dead_band(b)) return;
        if (!sc[b]) { sc[b] = true; cons.push_back(b); }
    };
    add_anc(anc);
    add_con(con);
    for (unsigned char ch : text) {
        for_anchor_candidates(encode(ch).target, add_anc);
        add_con(ch);
    }
    const int A = static_cast<int>(ancs.size());
    const int R = static_cast<int>(cons.size());

    // cell value -> candidate index (or -1): '@' targets, '^' park values
    // (encode is injective, so a park value names at most one constant),
    // and byte -> constant index for the '~' print check.
    std::vector<int> anc_at(btf::MOD, -1), park_at(btf::MOD, -1), con_of(256, -1);
    for (int i = 1; i < A; ++i) anc_at[ancs[i] + MAXV] = i;
    for (int i = 1; i < R; ++i) {
        park_at[encode(cons[i]).target + MAXV] = i;
        con_of[cons[i]] = i;
    }

    const long per   = static_cast<long>(btf::MOD) * A * R;
    const long total = (N + 1) * per;
    auto id = [&](int i, int c, int ai, int ri) {
        return ((static_cast<long>(i) * btf::MOD + (c + MAXV)) * A + ai) * R + ri;
    };
    std::vector<uint8_t>  vis(total, 0);
    std::vector<uint32_t> par(total);
    std::vector<char>     via(total);

    const int a0 = anc == ANC_NONE ? 0 : anc_at[anc + MAXV];
    const int r0 = con < 0 ? 0 : con_of[con];
    const long start = id(0, cell, a0, r0);
    vis[start] = 1;
    std::queue<uint32_t> q;
    q.push(static_cast<uint32_t>(start));
    long goal = -1;

    while (!q.empty()) {
        const long u = q.front();
        q.pop();
        long r = u;
        const int ri = static_cast<int>(r % R); r /= R;
        const int ai = static_cast<int>(r % A); r /= A;
        const int c  = static_cast<int>(r % btf::MOD) - MAXV; r /= btf::MOD;
        const int i  = static_cast<int>(r);
        if (i == N) { goal = u; break; }
        auto push = [&](long v, char op) {
            if (!vis[v]) {
                vis[v] = 1;
                par[v] = static_cast<uint32_t>(u);
                via[v] = op;
                q.push(static_cast<uint32_t>(v));
            }
        };

        const std::pair<int, char> nbr[] = {
            {wrap(c + 1),     '+'}, {wrap(c - 1),     '-'},
            {wrap(c * 3),     '*'}, {c / 3,           '/'},
            {wrap(c * 3 + 1), '('}, {wrap(c * 3 - 1), ')'},
        };
        for (auto [v, op] : nbr) push(id(i, v, ai, ri), op);
        push(id(i, 0, ai, ri), '>');
        if (anc_at[c + MAXV] >= 0)  push(id(i, c, anc_at[c + MAXV], ri), '@');
        if (ai > 0)                 push(id(i, ancs[ai], ai, ri), '_');
        if (park_at[c + MAXV] >= 0) push(id(i, c, ai, park_at[c + MAXV]), '^');
        for (int k = 0; k < 6; ++k) {            // fused step + anchor store
            const int v = btf::apply_step(c, k);
            if (anc_at[v + MAXV] >= 0)
                push(id(i, v, anc_at[v + MAXV], ri), btf::BUILD_STORE[k]);
        }

        const int b = static_cast<unsigned char>(text[i]);
        const btf::Enc e = encode(b);
        if (c == e.target)              push(id(i + 1, c, ai, ri), e.term);
        if (ri > 0 && cons[ri] == b)    push(id(i + 1, c, ai, ri), '~');
        if (e.term == '.') {                     // fused prints lower to PUTC
            for (int k = 0; k < 6; ++k)          // step on cell, print
                if (btf::apply_step(c, k) == e.target)
                    push(id(i + 1, e.target, ai, ri), btf::FUSED_PRINT[k]);
            if (ai > 0) {
                if (ancs[ai] == e.target)        // recall + print
                    push(id(i + 1, e.target, ai, ri), 'U');
                for (int k = 0; k < 6; ++k)      // step on anchor, print
                    if (btf::apply_step(ancs[ai], k) == e.target)
                        push(id(i + 1, e.target, ai, ri), btf::ANCREL_PRINT[k]);
            }
        }
    }

    // unreachable: '+' alone spans the ring, so every print is reachable
    if (goal < 0) return "";

    long r = goal;
    const int ri = static_cast<int>(r % R); r /= R;
    const int ai = static_cast<int>(r % A); r /= A;
    cell = static_cast<int>(r % btf::MOD) - MAXV;
    anc  = ai > 0 ? ancs[ai] : ANC_NONE;
    con  = ri > 0 ? cons[ri] : -1;

    std::string prog;
    for (long u = goal; u != start; u = par[u]) prog += via[u];
    for (std::size_t a = 0, z = prog.size() - 1; a < z; ++a, --z)
        std::swap(prog[a], prog[z]);
    return prog;
}

// Largest prefix of text[pos..] whose search fits MAX_STATES (always >= 1).
std::size_t chunk_len(const std::string& text, std::size_t pos, int anc, int con) {
    bool sa[btf::MOD] = {}, sc[256] = {};
    long A = 1, R = 1;
    if (anc != ANC_NONE) { sa[anc + MAXV] = true; ++A; }
    if (con >= 0)        { sc[con] = true; ++R; }
    std::size_t len = 0;
    while (pos + len < text.size()) {
        const int b = static_cast<unsigned char>(text[pos + len]);
        for_anchor_candidates(encode(b).target, [&](int v) {
            if (!sa[v + MAXV]) { sa[v + MAXV] = true; ++A; }
        });
        long r = R + (!dead_band(b) && !sc[b]);
        if (len > 0 &&
            static_cast<long>(len + 2) * btf::MOD * A * r > MAX_STATES)
            break;
        if (!dead_band(b)) sc[b] = true;
        R = r;
        ++len;
    }
    return len;
}

std::string translate(const std::string& text) {
    std::string prog;
    int cell = 0, anc = ANC_NONE, con = -1;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t len = chunk_len(text, pos, anc, con);
        prog += solve(text.substr(pos, len), cell, anc, con);
        pos += len;
    }
    return prog;
}

}  // namespace

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

    const std::string best = translate(text);

    // Wrap to keep lines readable; whitespace is ignored by the interpreter.
    constexpr std::size_t WIDTH = 72;
    for (std::size_t i = 0; i < best.size(); i += WIDTH)
        std::cout << best.substr(i, WIDTH) << '\n';
    return 0;
}
