# btf

Cells are 5-trit balanced ternary (range ±121, wrap mod 243).

Instead of unsigned 8-bit (0..255, mod 256).

Kept dense as `int8_t` in a `std::vector<Cell>` tape.

13 of the 256 `int8` codes (`-128..-122`, `+122..+127`) are unreachable.

Those 13 are contiguous in the *unsigned* byte view (`122..134`), reused as an
overlay namespace: a **2-trit balanced subcell** (±4, 9 codes `122..130`,
value = `u - 126`) plus **4 tags** (codes `131..134`). They are inert under
`+ - * /`: any arithmetic runs through `wrap()`, which folds the result back
into ±121, so only the overlay ops below create or touch them.

Adds five ops: `*` `/` are ×3 / ÷3 trit shifts, `?` extracts sign.

`:` and `;` do trit-string I/O (using `+0-` alphabet)

Overlay ops (interpreter-only; the JIT declines programs using them):
- `{` set subcell 0, `)` subcell +1, `(` subcell -1 (balanced wrap ±4), `}` print subcell as 2 trits.
- `@` select tag `T0..T3` (by repetition), `&` print current tag.
- `=` decode a tag to `1..4` in a normal cell (or `0` if not a tag) so `[ ]`/arithmetic can branch on it.

`+ - > < [ ] . ,` unchanged from original bf.

---

## Example (inspired [from](https://wiki.archlinux.org/title/Arch_is_the_best))

`out/txt2btf` generates a btf program that prints a string (text from args,
joined with spaces, or stdin):

```shell
./out/txt2btf "Arch is the best" | ./out/btf /dev/stdin
Arch is the best
```

See `examples/arch.btf` for the generated source. Which you can then round trip again:
```shell
cat ./examples/arch.btf | ./out/btf /dev/stdin; echo
```
