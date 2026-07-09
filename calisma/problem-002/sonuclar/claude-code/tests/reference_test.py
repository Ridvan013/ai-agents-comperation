#!/usr/bin/env python3
"""Randomised differential test of int2048 against Python's big integers.

Build the REPL first (``make repl`` or the CMake ``repl`` target), then run
this script. It feeds thousands of randomised operations through the REPL and
checks every result against Python.
"""
import os
import random
import subprocess
import sys

sys.set_int_max_str_digits(2_000_000)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def find_repl():
    for name in ("repl", "repl.exe"):
        for base in (ROOT, os.path.join(ROOT, "build")):
            path = os.path.join(base, name)
            if os.path.exists(path):
                return path
    sys.exit("repl executable not found; build it first (make repl).")


def rand_int(max_digits):
    n = random.randint(1, max_digits)
    s = "".join(random.choice("0123456789") for _ in range(n)).lstrip("0") or "0"
    if s != "0" and random.random() < 0.5:
        s = "-" + s
    return s


def expected(op, a, b):
    A, B = int(a), int(b)
    return {
        "+": lambda: A + B,
        "-": lambda: A - B,
        "*": lambda: A * B,
        "/": lambda: A // B,
        "%": lambda: A % B,
        "neg": lambda: -A,
        "<": lambda: 1 if A < B else 0,
        "==": lambda: 1 if A == B else 0,
        "add": lambda: A + B,
        "minus": lambda: A - B,
    }[op]()


def main():
    random.seed(20240607)
    ops = ["+", "-", "*", "/", "%", "neg", "<", "==", "add", "minus"]
    cases = []

    # A broad mix of magnitudes, including the recursive-division regime.
    for _ in range(6000):
        op = random.choice(ops)
        span = random.choice([6, 40, 300, 2000])
        a, b = rand_int(span), rand_int(span)
        if op in ("/", "%") and b.lstrip("-") == "0":
            b = "1"
        cases.append((op, a, b))

    # Hand-picked edge cases around zero and floor-division sign rules.
    cases += [
        ("+", "0", "0"), ("-", "5", "5"), ("*", "0", "-123"), ("neg", "0", "0"),
        ("/", "10", "3"), ("/", "-10", "3"), ("/", "10", "-3"), ("/", "-10", "-3"),
        ("%", "10", "3"), ("%", "-10", "3"), ("%", "10", "-3"), ("%", "-10", "-3"),
        ("/", "7", "1"), ("%", "-1", "1000000"),
    ]

    repl = find_repl()
    stdin = "\n".join(f"{op}\n{a}\n{b}" for op, a, b in cases) + "\n"
    out = subprocess.run([repl], input=stdin, capture_output=True,
                         text=True).stdout.split("\n")

    bad = 0
    for i, (op, a, b) in enumerate(cases):
        got = out[i].strip()
        exp = str(expected(op, a, b))
        if got != exp:
            bad += 1
            if bad <= 20:
                print(f"MISMATCH {op} a={a[:40]} b={b[:40]} exp={exp[:40]} got={got[:40]}")
    print(f"ran {len(cases)} cases, {bad} failures")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
