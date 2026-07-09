#!/usr/bin/env python3
"""002 int2048 test uretici - Python bigint = ground truth (floor bolme spec ile ayni).
Uretir: input.txt (op a b satirlari) + expected.txt (Python sonuclari).
Deterministik (seed sabit)."""
import random, os

random.seed(2048)
HERE = os.path.dirname(os.path.abspath(__file__))

def randbig(maxd):
    d = random.randint(1, maxd)
    s = "".join(random.choice("0123456789") for _ in range(d)).lstrip("0") or "0"
    if s != "0" and random.random() < 0.5:
        s = "-" + s
    return s

cases = []
# elle edge case'ler
edge = [
    ("+","0","0"),("-","0","0"),("*","0","999999999999"),("+","-5","5"),("-","5","5"),
    ("/","7","3"),("/","-7","3"),("/","7","-3"),("/","-7","-3"),
    ("%","7","3"),("%","-7","3"),("%","7","-3"),("%","-7","-3"),
    ("/","10","1"),("/","-1","1000000000"),("*","-1","-1"),("+","999999999","1"),
    ("-","1000000000","1"),("<","-5","5"),("==","0","-0"),(">=","123","123"),
    ("*","123456789012345678901234567890","987654321098765432109876543210"),
]
cases.extend(edge)

ops = ["+","-","*","/","%","<",">","==","!=","<=",">="]
for op in ops:
    for _ in range(30):
        # cesitli boyutlar
        md = random.choice([1, 3, 9, 20, 50, 200, 600])
        a = randbig(md); b = randbig(random.choice([1, 3, 9, 20, 50, 200, 600]))
        if op in ("/","%") and int(b) == 0:
            b = "1"
        cases.append((op, a, b))

lines_in, lines_exp = [], []
for op, a, b in cases:
    A, B = int(a), int(b)
    if op == "+": r = A + B
    elif op == "-": r = A - B
    elif op == "*": r = A * B
    elif op == "/": r = A // B          # Python floor bolme = spec
    elif op == "%": r = A % B           # Python floored mod
    elif op == "<": r = int(A < B)
    elif op == ">": r = int(A > B)
    elif op == "==": r = int(A == B)
    elif op == "!=": r = int(A != B)
    elif op == "<=": r = int(A <= B)
    elif op == ">=": r = int(A >= B)
    lines_in.append(f"{op} {a} {b}")
    lines_exp.append(str(r))

open(os.path.join(HERE, "input.txt"), "w").write("\n".join(lines_in) + "\n")
open(os.path.join(HERE, "expected.txt"), "w").write("\n".join(lines_exp) + "\n")
print(f"{len(cases)} test uretildi -> input.txt + expected.txt")
