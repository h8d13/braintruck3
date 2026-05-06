#!/usr/bin/env python3
"""A/B bench against a snapshot binary.

Usage:
    cp out/btf out/btf_<tag>          # save baseline before edit
    # edit code, ./build.sh
    ./bench.py out/btf_<tag>          # compare new vs snapshot

Reports min wall time over RUNS samples and (optionally) callgrind Ir.
"""

import os, subprocess, sys, time, tempfile, shutil

NEW  = './out/btf'
TMP  = 'tmp_bench'
RUNS = 15
USE_CALLGRIND = '--ir' in sys.argv
positional = [a for a in sys.argv[1:] if not a.startswith('--')]
OLD = positional[0] if positional else './out/btf_v1'

def write(path, content):
    with open(path, 'w') as f:
        f.write(content)

def time_run(binary, src):
    times = []
    for _ in range(RUNS):
        t0 = time.perf_counter()
        subprocess.run([binary, src], stdout=subprocess.DEVNULL, check=True)
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000)
    return min(times)

def ir_run(binary, src):
    f = tempfile.mktemp()
    subprocess.run(
        ['valgrind', '--tool=callgrind', f'--callgrind-out-file={f}', binary, src],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True
    )
    with open(f) as h:
        for line in h:
            if line.startswith('summary:'):
                ir = int(line.split()[1])
                break
    os.unlink(f)
    return ir

os.makedirs(TMP, exist_ok=True)

tests = {
    'mul N=50000':
        '> ++++++++++ <\n' + '+ * . > . <\n' * 50000,
    'clear+set N=200000':
        '[-]+++++\n' * 200000,
    'sub-move N=80000':
        '++++++++[->-<]\n' * 80000,
    'copy-2 N=40000':
        '++++++++[->+>+<<]\n' * 40000,
    'scan W=9000 N=100':
        '>+' * 9000 + ('[<]' + '>' * 9000) * 100,
    # New: multiplier MOVE: [->+++<] triples cell into right neighbor.
    'mul-move N=20000':
        '++++++++[->+++<]\n' * 20000,
    # New (C2): constant prop through SET via ADD/MUL3.
    # Without: SET 3, MUL3, MUL3, MUL3, PUTC (5 ops/iter).
    # With:    SET 81, PUTC (2 ops/iter, fully const-folded at compile time).
    'const-prop N=50000':
        '[-]+++***.\n' * 50000,
    # New (C3): [*] / [/] reach 0 in <=5 iters; replace with SET 0.
    # Plus dead-elim drops the preceding +++ since SET overwrites.
    'star-loop N=50000':
        '+++[*].\n' * 50000,

    # JIT-favoring (MUL3/DIV3-heavy; survive the const-fold pipeline)
    # `+` between `*` ops prevents fold-into-SET, so MUL3 stays in IR for runtime.
    # These are where BTF_JIT=1 actually engages.
    'mul3-x2 N=100000':       # 2 MUL3 per iter
        '+ * * .\n' * 100000,
    'mul3-x4 N=50000':        # 4 MUL3 per iter
        '+ * * + * * .\n' * 50000,
    'div3-chain N=80000':     # 2 DIV3 per iter, large initial value
        '+++++++++++ / / .\n' * 80000,
}

if not os.path.exists(OLD):
    print(f'snapshot {OLD} not found; cp out/btf out/btf_<tag> first', file=sys.stderr)
    sys.exit(1)

print(f'comparing  new={NEW}  old={OLD}')
metric = 'Ir' if USE_CALLGRIND else 'ms'
print(f'{"test":24s} {"old "+metric:>12s} {"new "+metric:>12s} {"speedup":>10s}')
for name, content in tests.items():
    path = f'{TMP}/{name.split()[0]}.btf'
    write(path, content)
    if USE_CALLGRIND:
        old = ir_run(OLD, path)
        new = ir_run(NEW, path)
    else:
        old = time_run(OLD, path)
        new = time_run(NEW, path)
    ratio = old / new if new > 0 else float('inf')
    if USE_CALLGRIND:
        print(f'{name:24s} {old:>12,d} {new:>12,d} {ratio:>9.2f}x')
    else:
        print(f'{name:24s} {old:>12.2f} {new:>12.2f} {ratio:>9.2f}x')
