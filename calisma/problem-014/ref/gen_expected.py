#!/usr/bin/env python3
"""014 Python Interpreter - beklenen cikti uretici.
python3'u spec-preamble ile calistirir (float print -> %.6f). Cikti = ground truth.
Kaynak: projdevbench/problem/014/testcases (basic + bigint).
HARIC: test14 (f-string icinde float -> python3 6-ondalik veremiyor; spec-uyumsuz).
Uretir: ref/<name>.in + ref/<name>.out
"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TC = os.path.join(HERE, "..", "..", "..", "projdevbench", "problem", "014", "testcases")
TC = os.path.abspath(TC)

PREAMBLE = r'''
import sys as _sys
try: _sys.set_int_max_str_digits(1000000)   # bigint: 4300-hane limitini kaldir
except Exception: pass
import builtins as _b
_op = _b.print
def _p(*a, **k):
    def f(x):
        if isinstance(x, bool): return "True" if x else "False"
        if isinstance(x, float): return "%.6f" % x
        return str(x)
    _op(*[f(x) for x in a], **k)
_b.print = _p
'''

# python3'un simplified-Python'dan ayristigi testler (oracle olamaz, haric):
#   test14: f-string icinde float (python3 '1.0', spec '1.000000')
#   test13: global'i 'global' bildirmeden degistirme (python3 UnboundLocalError; simplified izin veriyor)
HARIC = {"test14", "test13"}

tests = []
for sub in ("basic-testcases", "bigint-testcases"):
    d = os.path.join(TC, sub)
    if not os.path.isdir(d):
        continue
    for fn in sorted(os.listdir(d)):
        if fn.endswith(".in"):
            name = fn[:-3]
            if name in HARIC:
                continue
            tests.append((name, os.path.join(d, fn)))

uretilen = 0
for name, path in tests:
    src = open(path, encoding="utf-8", errors="replace").read()
    runner = PREAMBLE + "\n" + src
    env = dict(os.environ, PYTHONINTMAXSTRDIGITS="0")   # bigint: hane limitini parse-oncesi kaldir
    try:
        r = subprocess.run([sys.executable, "-c", runner], capture_output=True, text=True, timeout=30, env=env)
    except subprocess.TimeoutExpired:
        print(f"{name}: python3 TLE - atlaniyor")
        continue
    if r.returncode != 0:
        print(f"{name}: python3 HATA (atlaniyor): {r.stderr.strip().splitlines()[-1][:60] if r.stderr.strip() else '?'}")
        continue
    open(os.path.join(HERE, name + ".in"), "w", encoding="utf-8", newline="\n").write(src)
    open(os.path.join(HERE, name + ".out"), "w", encoding="utf-8", newline="\n").write(r.stdout)
    uretilen += 1

print(f"{uretilen}/{len(tests)} test icin beklenen cikti uretildi (test14 haric)")
