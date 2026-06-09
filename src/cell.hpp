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
    // Residue byte: resolve v mod 243 into the low range 0..242 instead of the
    // int8 two's-complement byte.  Lets '!' reach 122..134 (the bytes to_byte
    // skips) from the in-range cell values -121..-109.
    constexpr unsigned char to_byte_low() const {
        return static_cast<unsigned char>(v_ < 0 ? v_ + MOD : v_);
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
        // General case (AFFINE feeds up to |a*v + b| <= 81*121 + 121 here):
        // one truncating %, then one +-MOD nudge into the balanced range.
        x %= MOD;
        if (x > MAX_VAL)  x -= MOD;
        if (x < -MAX_VAL) x += MOD;
        return static_cast<storage_t>(x);
    }

private:
    storage_t v_ = 0;
};
