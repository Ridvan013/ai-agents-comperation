#!/usr/bin/env python3
"""Cross-check int2048 against Python's arbitrary-precision integers.

Runs the compiled test binary in `--stream` mode with random big operands
and compares each result to Python's own computation (Python uses floor
division / modulo, matching the assignment specification).
"""

import random
import subprocess
import sys
import time

EXE = sys.argv[1] if len(sys.argv) > 1 else "int2048_test.exe"


def rand_int(max_digits: int) -> int:
    d = random.randint(1, max_digits)
    n = int("".join(random.choice("0123456789") for _ in range(d)).lstrip("0") or "0")
    if random.random() < 0.5:
        n = -n
    return n


def py_result(a: int, op: str, b: int) -> int:
    if op == "+":
        return a + b
    if op == "-":
        return a - b
    if op == "*":
        return a * b
    if op == "/":
        return a // b  # Python // is floor division
    if op == "%":
        return a % b   # Python % matches floor semantics
    raise ValueError(op)


def run(a: int, op: str, b: int) -> str:
    inp = f"{a} {op} {b}\n"
    out = subprocess.run([EXE, "--stream"], input=inp, capture_output=True,
                         text=True)
    return out.stdout.strip()


def main() -> int:
    random.seed(2024)
    ops = ["+", "-", "*", "/", "%"]
    fails = 0
    total = 0
    for _ in range(2000):
        op = random.choice(ops)
        a = rand_int(400)
        b = rand_int(200)
        while op in ("/", "%") and b == 0:
            b = rand_int(200)
        total += 1
        got = run(a, op, b)
        want = str(py_result(a, op, b))
        if got != want:
            fails += 1
            print(f"FAIL: {a} {op} {b}\n  got={got}\n  want={want}")
            if fails > 20:
                break
    print(f"random big checks: total={total} fails={fails}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
