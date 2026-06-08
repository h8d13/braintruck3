# btf

Cells are 5-trit balanced ternary (range ±121, wrap mod 243).

Instead of unsigned 8-bit (0..255, mod 256).

Kept dense as `int8_t` in a `std::vector<Cell>` tape.

13 of the 256 `int8` codes (`-128..-122`, `+122..+127`) are simply unreachable
dead space — the cost of 243 < 256.

Adds five ops: `*` `/` are ×3 / ÷3 trit shifts, `?` extracts sign.

`:` and `;` do trit-string I/O (using `+0-` alphabet)

Constant register (one stored byte, for cheap reprints of a frequent char):
- `^` store current cell into the register, `~` print the register as a byte (cell + pointer untouched).

Anchor register (a second slot used as a build base):
- `@` store current cell into the anchor, `_` recall the anchor into the cell (then a small diff reaches a nearby value).

`+ - > < [ ] . ,` unchanged from original bf.

---

## Example (inspired [from](https://wiki.archlinux.org/title/Arch_is_the_best))

`out/txt2btf` generates a btf program that prints a string (text from args,
joined with spaces, or stdin):

```shell
./out/txt2btf "Arch is the best" | ./out/btf /dev/stdin
Arch is the best
```

**83 ops** vs **200+ ops** for hand-written original bf. `examples/arch_shorter.btf` pushes
it to  by also parking `114` in the anchor register, so the cluster
letters build via `_` + a small diff instead of a full Horner build.

`examples/arch.btf` is the older naive output (226 ops, one scratch+transfer per
char) kept for comparison; it embeds the equivalent original bf as a comment. It
round-trips the same way:
```shell
cat ./examples/arch.btf | ./out/btf /dev/stdin; echo
```

See [examples](./examples) for all the good shit.

---
