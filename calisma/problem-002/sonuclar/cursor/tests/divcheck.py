#!/usr/bin/env python3
"""Focused correctness check for large / balanced division and modulo."""

import random
import subprocess
import sys

sys.set_int_max_str_digits(2000000)

EXE = sys.argv[1] if len(sys.argv) > 1 else "int2048_test.exe"


def rand_int(digits: int, allow_neg: bool = True) -> int:
    n = int("".join(random.choice("0123456789") for _ in range(digits)).lstrip("0") or "0")
    if allow_neg and random.random() < 0.5:
        n = -n
    return n


def run(a: int, op: str, b: int) -> str:
    inp = f"{a} {op} {b}\n"
    out = subprocess.run([EXE, "--stream"], input=inp, capture_output=True,
                         text=True)
    return out.stdout.strip()


def check(a, op, b):
    want = str(a // b if op == "/" else a % b)
    got = run(a, op, b)
    if got != want:
        print(f"FAIL {op}: a_digits={len(str(abs(a)))} b_digits={len(str(abs(b)))}")
        print(f"  got ={got[:80]}...")
        print(f"  want={want[:80]}...")
        return False
    return True


def main() -> int:
    random.seed(7)
    cases = [
        (3000, 3000), (3000, 1500), (3000, 10), (3000, 1),
        (12000, 12000), (12000, 6000), (24000, 12000),
        (5000, 4999), (5000, 5000), (5001, 5000),
        (2000, 2000), (100, 100),
    ]
    ok = True
    for (da, db) in cases:
        for _ in range(3):
            a = rand_int(da)
            b = rand_int(db)
            while b == 0:
                b = rand_int(db)
            ok &= check(a, "/", b)
            ok &= check(a, "%", b)
    print("divcheck:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
