#!/usr/bin/env python3
"""Large-scale FFT multiplication precision check against Python."""

import random
import subprocess
import sys

sys.set_int_max_str_digits(4000000)

EXE = sys.argv[1] if len(sys.argv) > 1 else "int2048_test.exe"


def rand_str(digits: int) -> str:
    s = [random.choice("123456789")] + [random.choice("0123456789")
                                         for _ in range(digits - 1)]
    return "".join(s)


def main() -> int:
    random.seed(11)
    fails = 0
    for da, db in [(100, 100), (5000, 5000), (100000, 100000),
                   (250000, 250000), (250000, 3)]:
        a = rand_str(da)
        b = rand_str(db)
        sign = random.choice(["", "-"])
        expr = f"{sign}{a} * {b}\n"
        out = subprocess.run([EXE, "--stream"], input=expr,
                             capture_output=True, text=True).stdout.strip()
        want = str(int(sign + a) * int(b))
        ok = out == want
        print(f"mul {da}x{db}: {'PASS' if ok else 'FAIL'}")
        if not ok:
            fails += 1

    # Worst case for FFT precision: all-nines operands maximize coefficients.
    for d in [250000, 260000]:
        a = "9" * d
        expr = f"{a} * {a}\n"
        out = subprocess.run([EXE, "--stream"], input=expr,
                             capture_output=True, text=True).stdout.strip()
        want = str(int(a) * int(a))
        ok = out == want
        print(f"mul 9x{d} squared: {'PASS' if ok else 'FAIL'}")
        if not ok:
            fails += 1
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
