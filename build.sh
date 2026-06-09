#!/usr/bin/env bash
#
# BrainFuck (https://en.wikipedia.org/wiki/Brainfuck) 
# Converted to balanced ternary in C++.
# BrainTruck3. Uses +,0,- reps. Instead of the classic +, - representation.
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

echo "==> $CXX $CXXFLAGS -c src/btf.cpp -o out/btf.o"
$CXX $CXXFLAGS -c src/btf.cpp -o out/btf.o

echo "==> $CXX out/btf.o out/cell.o -o out/btf"
$CXX out/btf.o out/cell.o -o out/btf

echo "==> built out/btf"

# Single-TU tools sharing src/reach.hpp: txt2btf (text -> btf source),
# btf-build (value -> shortest build op-string), btf-dis (program -> annotated).
for tool in txt2btf btf_build btf_dis; do
    out="out/${tool//_/-}"
    echo "==> $CXX $CXXFLAGS src/$tool.cpp -o $out"
    $CXX $CXXFLAGS "src/$tool.cpp" -o "$out"
    echo "==> built $out"
done

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

echo "==> run tests with: bash tests/run.sh"
