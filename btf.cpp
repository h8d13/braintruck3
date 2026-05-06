#include "cell.hpp"
#include "ir.hpp"
#include "jit.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stack>
#include <cstdint>
#include <cstdlib>
#include <cstring>   // memchr / memrchr (memrchr is glibc; ifdef'd at use)

using ptr_t = std::size_t;

constexpr ptr_t TAPE_SIZE = 9999;

// Compiled IR.  ADD/MOVE collapse runs of +/-/>/< into a single signed
// delta.  JZ/JNZ store their matching op index directly in `arg`, so
// no separate jump table is needed.
// Op definition lives in ir.hpp (shared with jit.cpp).

static std::string strip_comments(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    bool in_comment = false;
    for (char c : raw) {
        if (in_comment) {
            if (c == '\n') in_comment = false;
            continue;
        }
        if (c == '#') { in_comment = true; continue; }
        out += c;
    }
    return out;
}

static std::vector<Op> compile(const std::string& src, std::string& err) {
    std::vector<Op> ops;
    std::stack<std::size_t> bracket_stack;
    std::int32_t add_run  = 0;
    std::int32_t move_run = 0;

    // flush_add folds into a preceding SET when possible:
    //   SET k; ADD m  ==>  SET (k+m)   (the fold is exact under the wrap;
    //   wrap distributes over + so the runtime gets the same value).
    // Constant-fold a cell-only op into a preceding SET.  Returns true if
    // folded (caller should not also push).  Cell-only kinds: ADD, MUL3,
    // DIV3, SIGN, SET (the latter being dead-SET elim).
    auto fold_into_set = [&](Op::Kind k, std::int32_t arg) -> bool {
        if (ops.empty() || ops.back().kind != Op::SET) return false;
        switch (k) {
            case Op::ADD:  ops.back().arg += arg;            return true;
            case Op::MUL3: ops.back().arg *= 3;              return true;
            case Op::DIV3: ops.back().arg /= 3;              return true;
            case Op::SIGN: {
                std::int32_t v = ops.back().arg;
                ops.back().arg = (v > 0) - (v < 0);          return true;
            }
            case Op::SET:  ops.back().arg  = arg;            return true;
            default:                                         return false;
        }
    };
    // Emit a cell-only op, folding into preceding SET when possible.
    auto emit_cell = [&](Op::Kind k, std::int32_t arg = 0) {
        if (!fold_into_set(k, arg)) ops.push_back({k, arg});
    };

    auto flush_add = [&]{
        if (add_run != 0) {
            emit_cell(Op::ADD, add_run);
            add_run = 0;
        }
    };
    auto flush_move = [&]{
        if (move_run != 0) ops.push_back({Op::MOVE, move_run});
        move_run = 0;
    };

    for (std::size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        switch (c) {
            case '+': flush_move(); ++add_run; break;
            case '-': flush_move(); --add_run; break;
            case '>': flush_add();  ++move_run; break;
            case '<': flush_add();  --move_run; break;
            case '*': flush_add(); flush_move(); emit_cell(Op::MUL3); break;
            case '/': flush_add(); flush_move(); emit_cell(Op::DIV3); break;
            case '?': flush_add(); flush_move(); emit_cell(Op::SIGN); break;
            case '.': flush_add(); flush_move(); ops.push_back({Op::PUTC,  0}); break;
            case ',': flush_add(); flush_move(); ops.push_back({Op::GETC,  0}); break;
            case ':': flush_add(); flush_move(); ops.push_back({Op::PUTTR, 0}); break;
            case ';': flush_add(); flush_move(); ops.push_back({Op::GETTR, 0}); break;
            case '[':
                flush_add(); flush_move();
                bracket_stack.push(ops.size());
                ops.push_back({Op::JZ, 0});  // patched at matching ']'
                break;
            case ']':
                flush_add(); flush_move();
                if (bracket_stack.empty()) {
                    err = "unmatched ']' at byte " + std::to_string(i);
                    return {};
                }
                {
                    std::size_t open = bracket_stack.top();
                    bracket_stack.pop();

                    // Peephole on the loop body (between JZ and the upcoming JNZ).
                    std::size_t body  = open + 1;
                    std::size_t blen  = ops.size() - body;
                    bool peeped = false;

                    // [-] / [+]  ==>  SET 0
                    if (blen == 1 && ops[body].kind == Op::ADD) {
                        ops.resize(open);
                        emit_cell(Op::SET, 0);
                        peeped = true;
                    }
                    // [<] / [>] (any stride)  ==>  SCAN d
                    else if (blen == 1 && ops[body].kind == Op::MOVE) {
                        std::int32_t d = ops[body].arg;
                        ops.resize(open);
                        ops.push_back({Op::SCAN, d});
                        peeped = true;
                    }
                    // [*] / [/]  ==>  SET 0.  Each shifts the cell by one trit
                    // per iter and reaches 0 within 5 iters (since 3^5 ≡ 0
                    // mod 243 in balanced ternary; div truncates toward 0).
                    else if (blen == 1 && (ops[body].kind == Op::MUL3 ||
                                           ops[body].kind == Op::DIV3)) {
                        ops.resize(open);
                        emit_cell(Op::SET, 0);
                        peeped = true;
                    }
                    // Drain pattern: [ ADD(-1) (MOVE d, ADD k)+ MOVE(-sum_d) ]
                    // for any nonzero k.  Three rewrite shapes:
                    //   pairs=1, |k|=1   ==>  MOVE_ADD/MOVE_SUB d        (drain in one op)
                    //   else             ==>  |k| copies of ADD_AT/SUB_AT per target,
                    //                         then SET 0 to drain source
                    else if (blen >= 4 && (blen - 2) % 2 == 0 &&
                             ops[body].kind == Op::ADD && ops[body].arg == -1) {
                        std::size_t pairs = (blen - 2) / 2;
                        std::vector<std::pair<std::int32_t, std::int32_t>> targets;
                        std::int32_t cum = 0;
                        bool ok = true;
                        for (std::size_t i = 0; i < pairs; ++i) {
                            const Op& mv = ops[body + 1 + 2*i];
                            const Op& ad = ops[body + 2 + 2*i];
                            if (mv.kind != Op::MOVE) { ok = false; break; }
                            if (ad.kind != Op::ADD || ad.arg == 0) {
                                ok = false; break;
                            }
                            cum += mv.arg;
                            targets.emplace_back(cum, ad.arg);
                        }
                        if (ok) {
                            const Op& last = ops[body + 1 + 2*pairs];
                            if (last.kind == Op::MOVE && last.arg == -cum) {
                                ops.resize(open);
                                if (pairs == 1 && (targets[0].second == +1 || targets[0].second == -1)) {
                                    if (targets[0].second == +1)
                                        ops.push_back({Op::MOVE_ADD, targets[0].first});
                                    else
                                        ops.push_back({Op::MOVE_SUB, targets[0].first});
                                } else {
                                    for (auto& [off, k] : targets) {
                                        std::int32_t mag = k < 0 ? -k : k;
                                        Op::Kind kind = (k > 0) ? Op::ADD_AT : Op::SUB_AT;
                                        for (std::int32_t i = 0; i < mag; ++i)
                                            ops.push_back({kind, off});
                                    }
                                    emit_cell(Op::SET, 0);
                                }
                                peeped = true;
                            }
                        }
                    }

                    if (!peeped) {
                        ops.push_back({Op::JNZ, static_cast<std::int32_t>(open)});
                        ops[open].arg = static_cast<std::int32_t>(ops.size() - 1);
                    }
                }
                break;
            default: break;  // whitespace / unknown ignored
        }
    }
    flush_add();
    flush_move();

    if (!bracket_stack.empty()) {
        err = "unmatched '[' at op " + std::to_string(bracket_stack.top());
        return {};
    }
    return ops;
}

// Helpers extracted to keep run()'s computed-goto region free of non-trivial
// destructors (GCC warns when CG could skip dtors of scoped objects).
static inline void exec_gettr(Cell& c) {
    std::string tok;
    std::cin >> tok;
    c = Cell::from_trits(tok);
}
static inline void exec_getc(Cell& c) {
    int ch = std::cin.get();
    c = Cell(ch == EOF ? 0 : ch);
}

// Computed-goto dispatch (GCC/Clang labels-as-values).  Each handler ends
// with NEXT() which jumps directly to the next op's handler   no shared
// dispatch loop, fewer pipeline bubbles, branch predictor learns per-handler
// successor patterns.  Order of `table` MUST match the Op::Kind enum.
static int run(const std::vector<Op>& ops) {
    std::vector<Cell> tape_vec(TAPE_SIZE);
    Cell* const tape = tape_vec.data();      // raw pointer: no operator[] indirection
    const Op* const op = ops.data();         // same for the IR
    const std::size_t n = ops.size();
    ptr_t p = 0;
    std::size_t pc = 0;

    if (n == 0) return 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    static const void* const table[] = {
        &&op_add, &&op_move, &&op_mul3, &&op_div3, &&op_sign,
        &&op_putc, &&op_getc, &&op_puttr, &&op_gettr,
        &&op_jz, &&op_jnz,
        &&op_set, &&op_move_add, &&op_move_sub,
        &&op_add_at, &&op_sub_at, &&op_scan
    };
#define NEXT() do { if (++pc >= n) return 0; \
                    goto *table[op[pc].kind]; } while (0)

    goto *table[op[pc].kind];

op_add:      tape[p] += op[pc].arg;                                   NEXT();
op_move:     p += static_cast<ptr_t>(op[pc].arg);                     NEXT();
op_mul3:     tape[p] *= 3;                                            NEXT();
op_div3:     tape[p] /= 3;                                            NEXT();
op_sign:     tape[p].set_sign();                                      NEXT();
op_putc:     std::cout.put(static_cast<char>(tape[p].to_byte()));     NEXT();
op_getc:     exec_getc(tape[p]);                                      NEXT();
op_puttr:    std::cout << tape[p].trits();                            NEXT();
op_gettr:    exec_gettr(tape[p]);                                     NEXT();
op_jz:       if ( tape[p].is_zero()) pc = static_cast<std::size_t>(op[pc].arg); NEXT();
op_jnz:      if (!tape[p].is_zero()) pc = static_cast<std::size_t>(op[pc].arg); NEXT();
op_set:      tape[p] = Cell(op[pc].arg);                              NEXT();
op_move_add:
    tape[p + static_cast<ptr_t>(op[pc].arg)] += static_cast<int>(tape[p].value());
    tape[p] = Cell();                                                 NEXT();
op_move_sub:
    tape[p + static_cast<ptr_t>(op[pc].arg)] -= static_cast<int>(tape[p].value());
    tape[p] = Cell();                                                 NEXT();
op_add_at:
    tape[p + static_cast<ptr_t>(op[pc].arg)] += static_cast<int>(tape[p].value());
    NEXT();
op_sub_at:
    tape[p + static_cast<ptr_t>(op[pc].arg)] -= static_cast<int>(tape[p].value());
    NEXT();
op_scan: {
    std::int32_t d = op[pc].arg;
    // Stride ±1 covers the [<] / [>] case where memchr-class SIMD scans
    // dominate.  glibc's memchr / memrchr can chew 16-32 bytes/cycle.
    if (d == 1) {
        const std::int8_t* base = reinterpret_cast<const std::int8_t*>(tape);
        const std::int8_t* hit = static_cast<const std::int8_t*>(
            std::memchr(base + p, 0, TAPE_SIZE - p));
        p = hit ? static_cast<ptr_t>(hit - base) : TAPE_SIZE - 1;
    } else if (d == -1) {
#ifdef __GLIBC__
        const std::int8_t* base = reinterpret_cast<const std::int8_t*>(tape);
        const std::int8_t* hit = static_cast<const std::int8_t*>(
            ::memrchr(base, 0, p + 1));
        p = hit ? static_cast<ptr_t>(hit - base) : 0;
#else
        while (!tape[p].is_zero()) --p;
#endif
    } else {
        while (!tape[p].is_zero()) p += static_cast<ptr_t>(d);
    }
    NEXT();
}
#undef NEXT
#pragma GCC diagnostic pop
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <program.btf>\n";
        return 1;
    }
    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << "cannot open " << argv[1] << '\n';
        return 1;
    }
    std::string raw{std::istreambuf_iterator<char>(f), {}};
    std::string src = strip_comments(raw);

    std::string err;
    std::vector<Op> ops = compile(src, err);
    if (!err.empty()) {
        std::cerr << err << '\n';
        return 1;
    }

    // BTF_JIT=1: compile IR to native x86_64 machine code and run that,
    // but only when the IR is JIT-compatible.  SCAN, SIGN, and trit-string
    // I/O ops fall through to the interpreter (memchr-backed scan, etc.).
    if (const char* env = std::getenv("BTF_JIT"); env && env[0] != '0') {
        if (BtfJit::can_compile(ops)) {
            try {
                BtfJit jit;
                auto fn = jit.compile(ops);
                std::vector<std::int8_t> tape(TAPE_SIZE, 0);
                fn(tape.data());
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "JIT failed: " << e.what() << '\n';
                return 1;
            }
        }
        // Fall through to interpreter for unsupported ops.
    }

    return run(ops);
}
