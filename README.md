# btf

Cells are 5-trit balanced ternary (range ±121, wrap mod 243) instead of unsigned
8-bit. Kept dense as `int8_t` in a `std::vector<Cell>` tape. A cell holds 243 of
the 256 byte values; `.` (two's-complement byte) prints `0..121` + `135..255`,
and `!` (residue byte, `v mod 243`) prints `0..242`, so between them every byte
is reachable. `txt2btf` picks per char; the `122..134` band needs `!`.

Extra ops: `*` `/` are ×3 / ÷3 trit shifts, `(` `)` are fused Horner steps
(`×3+1` / `×3-1`, one op per balanced-ternary trit), `?` extracts sign, `:` `;`
do trit-string I/O (`+0-` alphabet), `!` is the residue print. Two one-byte
registers:
- constant (`^` store, `~` reprint as byte, cell untouched): a frequent char.
- anchor (`@` store, `_` recall into cell): a build base a short reach away.

`+ - > < [ ] . ,` unchanged from original bf.

## Example ([from](https://wiki.archlinux.org/title/Arch_is_the_best))

Hand-written brainfuck, "Arch is the best!", 203 ops:

```brainfuck
++>++++++>+++++<+[>[->+<]<->++++++++++<]>>.<[-]>[-<++>]
<----------------.---------------.+++++.<+++[-<++++++++++>]<.
>>+.++++++++++.<<.>>+.------------.---.<<.>>---.
+++.++++++++++++++.+.<<+.[-]++++++++++.
```
Same but with our new op types.
```brainfuck
+(()(@>+())^_()._-.*._+).~+._.~+.)./).~/)._)._.+.-)/.
```

`txt2btf` reaches each char by a BFS over `+ - * / ( )`, so a letter lands as a
fused-Horner build or a multiply off the value already in the cell (`115` is
`+(()(`) rather than a from-zero `*`-build. 

You can test the round trip easily.

```shell
./out/txt2btf "Arch is the best!" | ./out/btf /dev/stdin
Arch is the best!
```

Some edge cases:

`examples/lazy_zebra.btf` shows `!` printing `z` (byte 122), which `.` cannot reach.

See [examples](./examples) for all the good shit.

## Authoring tools

Hand-writing btf is mostly "get a cell to value N, then print". Two helpers:

```shell
./out/btf-build 65          # ++(()   shortest ops to build a value
./out/btf-build -c A        # A  ++(().   build-and-print a char
./out/btf-dis examples/cafe.btf   # annotate a program op by op
```

`btf-build` exposes the translator's BFS for one value; `btf-dis` traces cell
values through straight-line code (stopping at loops/input) so `+(()(.` reads as
"build 65, print 'A'". 

Tests: `bash tests/run.sh`.
See also native [tools](./tools)
