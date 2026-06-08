#!/usr/bin/env bash
#
# BrainFuck (https://en.wikipedia.org/wiki/Brainfuck) 
# Converted to balanced ternary in C++.
# BrainTruck3. Uses +,0,- reps. Instead of the classic +, - representation.
# This notably speeds up a lot of operations (MUL3 and DIV3 especially using JIT).
#
# ⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⠀⠀⠀⠀⠀⣀⣠⡤⠴⠶⠛⠛⢿⣶⣦⣄⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣤⣴⣶⣟⣻⣟⣉⣉⣉⣉⠉⠛⠒⠺⡍⠁⠀⠸⡄⠀⠀⠀⠐⡄⠉⢧⠈⠉⠓⠲⠤⣄⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣖⡿⠟⠛⠋⠉⠉⢨⣿⣯⠍⣿⡏⠩⡭⢿⡖⢲⣾⡄⠀⠀⢻⡄⠀⠀⠀⢣⠀⠸⡔⠒⠶⠤⣤⣀⣀⡉⠑⠒⠦⠤⣄⣀⡀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⠟⠋⠀⠀⠀⠀⠀⠀⠀⣸⡟⡇⠀⣿⣡⣠⡇⠀⢧⠸⡝⡿⡄⠀⠀⢷⠀⠀⠀⠸⡄⠀⣿⠲⢦⡤⣤⣀⣉⣉⣛⠶⠶⣤⣀⣈⠉⢻⣶⡀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣾⠁⠀⠀⠀⠀⠀⠀⠀⠀⣀⣿⢠⡇⣴⣿⠛⠉⠀⠀⢸⠀⡇⢹⣻⣄⣤⢼⣆⠀⠀⠀⢧⠀⢸⡀⠀⣧⠀⠀⠈⢯⠉⠛⢳⡖⠶⠬⣭⣓⣿⡇⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠃⠀⢀⣀⣀⣤⣤⡴⢖⣫⣿⡇⠸⣧⠿⣿⢤⣾⣀⣀⣼⠀⣷⢸⡏⠹⣌⠙⢿⣝⢦⡀⠸⡄⠘⣇⠀⠸⡆⠀⠀⠈⣇⠀⠀⣷⠀⠀⢹⠀⡇⣷⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⣀⡶⣲⡾⠭⡿⢛⣓⣛⣋⣉⢩⡾⡁⠀⠀⠒⠋⠀⠀⠀⠀⠀⠀⢿⣾⡇⠈⠉⠳⡄⠹⣎⢷⡀⢳⠀⢻⠀⠀⢿⡀⠀⠀⢹⡄⠀⢸⡆⠀⠘⣇⢿⢻⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⢠⣾⣿⣤⡼⠶⠚⠋⠉⠉⠀⠀⣹⣾⠀⢣⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣼⢿⣇⠀⠀⠀⡇⠀⣿⠘⡇⠈⡆⠘⣇⠀⠘⣧⠀⠀⠈⣧⠀⠀⢿⠀⠀⢿⢸⢸⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⣾⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⠀⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⣩⡟⠀⡿⣄⣀⣴⠇⢀⡏⣸⠇⠀⠹⡄⠸⡄⠀⠸⡄⠀⠀⢸⡀⠀⠘⣇⠀⢸⣼⣾⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⢿⡧⠤⠤⠴⣶⣶⠞⠓⠒⠒⠒⣿⢸⢀⣸⠀⠀⠀⠀⠀⠀⢀⣴⣾⠿⠛⠛⠒⢦⡄⠀⣠⢞⡴⠋⣀⣀⣀⠙⢦⣳⣄⣀⣳⣤⣤⣤⣵⣴⢶⣿⣶⣞⠛⠋⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠸⣇⠀⢰⣲⣿⣿⣶⣶⣶⡆⠀⢹⠸⡟⢿⡄⠀⠀⠀⠀⢠⡿⠋⠀⠀⠀⢀⣀⣀⣽⠺⠕⠋⠉⠉⠉⢹⡍⠉⢀⣭⡍⠉⠀⣸⢁⡽⢻⣿⠟⣿⠛⢿⡆⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠻⣦⣭⣭⣽⣿⣿⣿⣿⡯⠀⢸⣆⣇⣨⡇⠀⠀⢀⣀⣿⣁⣀⣠⠶⠛⠉⠁⠀⠹⣿⡻⠟⡿⠶⣒⣾⣇⣨⣯⣿⣗⣒⣾⣯⠏⠀⡘⣇⢰⣷⣴⣼⢿⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⣴⡲⣤⢄⣀⣀⣈⣉⣹⣿⣿⣿⣿⣿⣿⡛⠋⣽⠋⠀⢈⡽⠿⡛⠛⠓⠦⣄⠀⢹⣧⣶⣻⣿⠿⠿⠾⠿⠿⠵⠀⣼⠸⣿⢰⡄⣿⢸⣾⣿⣿⣿⡏⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⢿⢃⡘⠿⠿⢿⣷⠆⠀⡄⠐⠲⢻⣦⣸⣗⡾⠽⠿⠃⡞⢀⡞⠁⠀⣀⣀⠈⢳⠘⣿⡈⣿⡄⠀⣠⣆⣀⣤⣶⣚⣫⡍⢹⡸⣟⡿⣾⡸⣿⣻⡿⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⢸⣮⣿⠲⠤⠤⣼⡃⠀⠛⠚⠲⠾⠏⢹⠠⠴⢾⡆⢰⡇⡞⠀⣠⡞⢡⠽⢦⠈⡇⢹⣿⣿⣶⣿⠿⠷⠛⠋⠈⢷⣄⣷⣈⣷⣤⡾⠋⠙⠋⠉⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠙⠒⠦⢽⣿⣃⡖⠲⢶⠀⠀⠀⢻⢀⣠⣴⣧⣸⣿⠁⢀⡟⢀⠀⠀⢸⠆⣇⣀⡿⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⢿⣝⡻⢶⡶⢤⣼⡧⠤⠴⢿⠋⢸⠀⠘⣇⠸⡄⣠⡿⢰⡇⠈⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠀⠀⠀⠀⠀⠀⠀⣇⢸⡄⠀⠙⠶⠝⠋⣠⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢧⣽⣦⣄⣠⣴⠾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤
# ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿
# ⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿

set -euo pipefail

cd "$(dirname "$0")"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -O2 -Wall -Wextra -Wpedantic}"

mkdir -p out

# btf: compile each TU to .o, then link.  Demonstrates the two-stage
# C++ build (compile + link) explicitly rather than hiding it.
echo "==> $CXX $CXXFLAGS -c src/cell.cpp -o out/cell.o"
$CXX $CXXFLAGS -c src/cell.cpp -o out/cell.o

echo "==> $CXX $CXXFLAGS -c src/jit.cpp -o out/jit.o"
$CXX $CXXFLAGS -c src/jit.cpp -o out/jit.o

echo "==> $CXX $CXXFLAGS -c src/btf.cpp -o out/btf.o"
$CXX $CXXFLAGS -c src/btf.cpp -o out/btf.o

echo "==> $CXX out/btf.o out/cell.o out/jit.o -o out/btf"
$CXX out/btf.o out/cell.o out/jit.o -o out/btf

echo "==> built out/btf"

# txt2btf: single-TU tool, text -> btf source generator.
echo "==> $CXX $CXXFLAGS src/txt2btf.cpp -o out/txt2btf"
$CXX $CXXFLAGS src/txt2btf.cpp -o out/txt2btf

echo "==> built out/txt2btf"

# btf-native filters: the logic is the btf program itself; the out/ wrapper is
# a thin shim over out/btf.  text2trits <-> trits2text round-trip exactly.
for tool in trits2text text2trits; do
    cp "tools/$tool.btf" "out/$tool.btf"
    cat > "out/$tool" <<EOF
#!/bin/sh
exec "\$(dirname "\$0")/btf" "\$(dirname "\$0")/$tool.btf"
EOF
    chmod +x "out/$tool"
    echo "==> built out/$tool (btf-native)"
done
