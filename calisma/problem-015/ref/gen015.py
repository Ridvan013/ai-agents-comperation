#!/usr/bin/env python3
"""015 File Storage - test uretici. Python multimap = ground truth.
insert/delete/find (index<=64 byte string, value non-negative int, (index,value) essiz).
Uretir: ref/input.txt (n + komutlar) + ref/expected.txt (find ciktilari)."""
import random, os, bisect
from collections import defaultdict

random.seed(1515)
HERE = os.path.dirname(os.path.abspath(__file__))

# index havuzu (bazilari cok tekrarli -> stres)
idx_pool = [f"idx{i}" for i in range(1, 300)] + \
           [f"popular{i}" for i in range(1, 6)] * 40 + \
           ["".join(random.choice("abcXYZ_0123") for _ in range(random.randint(1, 20))) for _ in range(200)]

N = 12000
cmds = []
mm = defaultdict(list)   # index -> sorted list (essiz)
exp = []

for _ in range(N):
    r = random.random()
    if r < 0.5:   # insert
        idx = random.choice(idx_pool); val = random.randint(0, 2000000000)
        cmds.append(f"insert {idx} {val}")
        i = bisect.bisect_left(mm[idx], val)
        if i == len(mm[idx]) or mm[idx][i] != val:
            mm[idx].insert(i, val)
    elif r < 0.72:  # delete (bazen olmayan)
        idx = random.choice(idx_pool)
        if mm[idx] and random.random() < 0.7:
            val = random.choice(mm[idx])
        else:
            val = random.randint(0, 2000000000)   # muhtemelen yok
        cmds.append(f"delete {idx} {val}")
        i = bisect.bisect_left(mm[idx], val)
        if i < len(mm[idx]) and mm[idx][i] == val:
            mm[idx].pop(i)
    else:   # find
        idx = random.choice(idx_pool)
        cmds.append(f"find {idx}")
        exp.append(" ".join(map(str, mm[idx])) if mm[idx] else "null")

with open(os.path.join(HERE, "input.txt"), "w", newline="\n") as f:
    f.write(f"{N}\n" + "\n".join(cmds) + "\n")
with open(os.path.join(HERE, "expected.txt"), "w", newline="\n") as f:
    f.write("\n".join(exp) + "\n")
print(f"{N} komut, {len(exp)} find -> input.txt + expected.txt")
