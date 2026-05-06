# btf

Cells are 5-trit balanced ternary (range ±121, wrap mod 243) instead of unsigned 8-bit (0..255, mod 256).

Kept dense as `int8_t` in a `std::vector<Cell>` tape.

Adds five ops: `*` `/` are ×3 / ÷3 trit shifts, `?` extracts sign (-1 / 0 / +1).

`:` and `;` do trit-string I/O ("+0-" alphabet)

`+ - > < [ ] . ,` unchanged.

