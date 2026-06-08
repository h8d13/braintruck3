#pragma once

#include <cstdint>
#include <string>

class Cell {
public:
    using storage_t = std::int8_t;        // tape density: exact 1 byte
    using fast_t    = std::int_fast8_t;   // register-fast working width
    using wide_t    = std::int_fast16_t;  // wrap-arithmetic intermediate

    static constexpr fast_t MAX_VAL = 121;   // (3^5 - 1) / 2 (-121...121)
    static constexpr wide_t MOD     = 243;   // 3^5

    constexpr Cell() = default;
    constexpr explicit Cell(int x) : v_(wrap(x)) {}

    constexpr fast_t value() const { return v_; }
    constexpr bool   is_zero() const { return v_ == 0; }
    constexpr unsigned char to_byte() const {
        return static_cast<unsigned char>(v_);
    }

    Cell& operator++()      { v_ = wrap(v_ + 1); return *this; }
    Cell& operator--()      { v_ = wrap(v_ - 1); return *this; }
    Cell& operator+=(int k) { v_ = wrap(static_cast<wide_t>(v_) + k); return *this; }
    Cell& operator-=(int k) { v_ = wrap(static_cast<wide_t>(v_) - k); return *this; }
    Cell& operator*=(int k) { v_ = wrap(static_cast<wide_t>(v_) * k); return *this; }
    Cell& operator/=(int k) { v_ = static_cast<storage_t>(v_ / k); return *this; }

    void set_sign() {
        v_ = static_cast<storage_t>((v_ > 0) - (v_ < 0));
    }

    std::string trits() const;
    static Cell from_trits(const std::string& s);

    // --- Overlay namespace in the 13 codes the 5-trit range never produces ---
    // In unsigned view those codes are contiguous: 122..134. They carry a
    // 2-trit balanced subcell (±4) in 122..130 (value = u - 126) and 4 tags
    // in 131..134 (tag = u - 131). Inert under + - * / : wrap() folds any
    // arithmetic result back into ±121, so only the { ) ( } @ & ops create
    // or touch them.
    static constexpr int SPEC_LO  = 122;   // first overlay code (unsigned)
    static constexpr int SUB_HI   = 130;   // last subcell code
    static constexpr int SUB_ZERO = 126;   // subcell value 0
    static constexpr int TAG_LO   = 131;   // first tag code
    static constexpr int SPEC_HI  = 134;   // last overlay code

    constexpr unsigned ubyte()      const { return static_cast<unsigned char>(v_); }
    constexpr bool     is_special() const { return ubyte() >= SPEC_LO && ubyte() <= SPEC_HI; }
    constexpr bool     is_sub()     const { return ubyte() >= SPEC_LO && ubyte() <= SUB_HI; }
    constexpr bool     is_tag()     const { return ubyte() >= TAG_LO  && ubyte() <= SPEC_HI; }
    constexpr int      sub_value()  const { return static_cast<int>(ubyte()) - SUB_ZERO; }
    constexpr int      tag()        const { return static_cast<int>(ubyte()) - TAG_LO; }

    // Construct from a literal storage byte, bypassing wrap() (which would
    // otherwise fold an overlay code back into ±121).
    static constexpr Cell from_raw(storage_t raw) { Cell c; c.v_ = raw; return c; }

    // Subcell value wraps balanced mod 9 (±4), mirroring the cell's mod-243.
    static constexpr int sub_wrap(int s) { return ((s + 4) % 9 + 9) % 9 - 4; }
    static constexpr Cell make_sub(int s) {
        return from_raw(static_cast<storage_t>(SUB_ZERO + sub_wrap(s)));
    }
    static constexpr Cell make_tag(int t) {
        return from_raw(static_cast<storage_t>(TAG_LO + ((t % 4) + 4) % 4));
    }

    std::string sub_trits() const;   // subcell as 2 balanced trits, e.g. "+-"

    // Range-check via unsigned reinterpretation (one cmp).
    // Hot inputs (cell ± 1, cell × 3) all fit in |x| <= MAX + MOD, so a
    // single ±MOD adjustment after that branch suffices.
    // Falls back to the general formula for large-arg case.

    static constexpr storage_t wrap(wide_t x) {
        if (static_cast<std::uint32_t>(x + MAX_VAL) <=
            static_cast<std::uint32_t>(2 * MAX_VAL)) {
            return static_cast<storage_t>(x);
        }
        if (x > MAX_VAL && x <= MAX_VAL + MOD)
            return static_cast<storage_t>(x - MOD);
        if (x < -MAX_VAL && x >= -MAX_VAL - MOD)
            return static_cast<storage_t>(x + MOD);
        x = ((x + MAX_VAL) % MOD + MOD) % MOD - MAX_VAL;
        return static_cast<storage_t>(x);
    }

private:
    storage_t v_ = 0;
};
