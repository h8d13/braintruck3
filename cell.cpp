#include "cell.hpp"

std::string Cell::trits() const {
    if (v_ == 0) return "0";
    std::string out;
    wide_t x = v_;
    while (x != 0) {
        wide_t r = ((x % 3) + 3) % 3;
        if      (r == 0) { out += '0'; x =  x      / 3; }
        else if (r == 1) { out += '+'; x = (x - 1) / 3; }
        else             { out += '-'; x = (x + 1) / 3; }
    }
    return std::string(out.rbegin(), out.rend());
}

Cell Cell::from_trits(const std::string& s) {
    wide_t v = 0;
    for (char c : s) {
        v *= 3;
        if      (c == '+') v += 1;
        else if (c == '-') v -= 1;
    }
    return Cell(static_cast<int>(v));
}
