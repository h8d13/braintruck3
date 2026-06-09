#!/usr/bin/env bash
#
# Test suite for the btf toolchain: interpreter, translator (txt2btf),
# constant builder (btf-build), disassembler (btf-dis), and the trit filters.
# Run after build.sh.  Exits nonzero on any failure.

set -uo pipefail
cd "$(dirname "$0")/.."

BTF=out/btf
T2B=out/txt2btf
BLD=out/btf-build
DIS=out/btf-dis

pass=0
fail=0
check() {  # desc  expected  actual
    if [ "$2" = "$3" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        printf 'FAIL: %s\n  expected: [%s]\n  actual:   [%s]\n' "$1" "$2" "$3"
    fi
}

# Everything built?
for bin in "$BTF" "$T2B" "$BLD" "$DIS"; do
    [ -x "$bin" ] || { echo "missing $bin; run build.sh first"; exit 2; }
done

# --- txt2btf: text round-trips through the interpreter --------------------
echo "== txt2btf round-trip =="
for s in "Arch is the best!" "Hello, World!" "lazy zebra" "café" "hi!" "12.5%" "a" 'PMTDLR &={}'; do
    got=$(printf '%s' "$s" | "$T2B" | "$BTF" /dev/stdin)
    check "txt2btf '$s'" "$s" "$got"
done
# stdin multi-line
got=$(printf 'one\ntwo\n' | "$T2B" | "$BTF" /dev/stdin)
check "txt2btf stdin multiline" "$(printf 'one\ntwo\n')" "$got"

# --- btf-build: built value prints as the requested byte (0..121) ---------
echo "== btf-build correctness (cell 0..121) =="
for v in 0 1 32 33 65 99 115 121 10; do
    exp=$(printf '%02x' "$v")
    got=$(printf '%s.' "$($BLD "$v")" | "$BTF" /dev/stdin | xxd -p | tr -d '\n')
    check "btf-build $v" "$exp" "$got"
done
# -f FROM: reach from an existing cell value (build 32, then reach 32->99)
got=$(printf '%s%s.' "$($BLD 32)" "$($BLD -f 32 99)" | "$BTF" /dev/stdin)
check "btf-build -f 32 99 (prints c)" "c" "$got"

# --- btf-build -c: build-and-print snippet per char -----------------------
echo "== btf-build -c per-char snippet =="
while IFS=$'\t' read -r ch ops; do
    got=$(printf '%s' "$ops" | "$BTF" /dev/stdin)
    check "btf-build -c '$ch'" "$ch" "$got"
done < <("$BLD" -c 'Az !~')

# --- btf-dis: annotates known programs ------------------------------------
echo "== btf-dis annotations =="
dis=$("$DIS" examples/cafe.btf)
check "dis traces build 'c'"   "yes" "$(echo "$dis" | grep -q "print 'c'" && echo yes)"
check "dis shows UTF-8 195"    "yes" "$(echo "$dis" | grep -q "print 195" && echo yes)"
# fused families: step+store, step+print, anchor-relative print
disf=$("$DIS" examples/arch_shorter.btf)
check "dis fused store anchor" "yes" "$(echo "$disf" | grep -q "anchor = cell (115)" && echo yes)"
check "dis fused print 'A'"    "yes" "$(echo "$disf" | grep -q "cell\*3-1, print 'A'" && echo yes)"
check "dis anchor-rel 'r'"     "yes" "$(echo "$disf" | grep -q "anchor-1, print 'r'" && echo yes)"
# loop program: degrades gracefully, no crash, shows the loop header
disl=$(printf '+++[>+(<-]>.' | "$DIS" -)
check "dis loop header"        "yes" "$(echo "$disl" | grep -q "while cell != 0:" && echo yes)"
check "dis loop exit code"     "0"   "$?"

# --- trit filters: round-trip 1..242 (0 is the loop/EOF sentinel) ---------
echo "== text2trits | trits2text round-trip =="
for s in "Hi z" "café" "Arch"; do
    got=$(printf '%s' "$s" | "$BTF" out/text2trits.btf | "$BTF" out/trits2text.btf)
    check "trit round-trip '$s'" "$s" "$got"
done

# --- loop + fused op programs produce expected bytes -----------------------
echo "== loop/fused op programs =="
declare -A LOOP_PROGS=(
    ['+++[>+(<-]>.']='34'
    ['++++[>)<-]>+.']='d9'
    ['+(()(.[-]']='73'
    ['+++[>(R<-]']='0214c3'
)
for prog in "${!LOOP_PROGS[@]}"; do
    printf '%s' "$prog" > /tmp/btf_loop.btf
    got=$("$BTF" /tmp/btf_loop.btf | xxd -p | tr -d '\n')
    check "loop '$prog'" "${LOOP_PROGS[$prog]}" "$got"
done

# --- example files all run -----------------------------------------------
echo "== example files execute =="
for f in examples/*.btf; do
    "$BTF" "$f" >/dev/null 2>&1
    check "runs $(basename "$f")" "0" "$?"
done

echo
echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
