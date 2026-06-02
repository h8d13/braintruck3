# bt3

Cells are 5-trit balanced ternary (range ±121, wrap mod 243).

Instead of unsigned 8-bit (0..255, mod 256).

Kept dense as `int8_t` in a `std::vector<Cell>` tape.

13 of the 256 `int8` codes (`-128..-122`, `+122..+127`) are unreachable.

But could be used somehow?

Adds five ops: `*` `/` are ×3 / ÷3 trit shifts, `?` extracts sign (-1 / 0 / +1).

`:` and `;` do trit-string I/O (using "+0-" alphabet)

`+ - > < [ ] . ,` unchanged from original bt.

---
