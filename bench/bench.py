#!/usr/bin/env python3
"""Tiny throughput stress test for the btf interpreter.

Builds a loop fixture (a 121-count counter whose body is K MUL3 ops), runs it a
few times, and reports M-ops/s and us/op from the *executed* IR op count. The
body dominates compile, so this measures the interpreter's dispatch/exec hot
path rather than parsing.

Usage:  python3 bench/bench.py [K]      (K = MUL3 ops per iteration, default 100k)
"""
import os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
BTF  = os.path.join(HERE, "..", "out", "btf")

ITERS = 121                                                  # counter maxes at +121
K     = int(sys.argv[1]) if len(sys.argv) > 1 else 100_000   # MUL3 ops per iteration
REPS  = 5

# counter = 121; each iteration: > (move), K * (MUL3), < (move), - (dec).
fixture = "+" * ITERS + "[>" + "*" * K + "<-]"
# executed IR ops: init ADD + 121 cycles of (JZ MOVE K*MUL3 MOVE ADD JNZ) + final JZ
ops = ITERS * (K + 5) + 2

path = "/tmp/btf_bench.btf"
with open(path, "w") as f:
    f.write(fixture)

if not os.path.exists(BTF):
    sys.exit(f"missing {BTF} -- run ./build.sh first")

def run_once(jit):
    env = {**os.environ, "BTF_JIT": "1" if jit else "0"}
    t = time.perf_counter()
    subprocess.run([BTF, path], check=True, stdout=subprocess.DEVNULL, env=env)
    return time.perf_counter() - t

print(f"fixture : {ITERS} iters x {K:,} MUL3   ({ops:,} executed ops)")
for label, jit in (("interp", False), ("jit", True)):
    best = min(run_once(jit) for _ in range(REPS))
    print(f"{label:7}: {best * 1e3:7.1f} ms   "
          f"{ops / best / 1e6:8,.1f} M-ops/s   {best / ops * 1e6:.4f} us/op")
