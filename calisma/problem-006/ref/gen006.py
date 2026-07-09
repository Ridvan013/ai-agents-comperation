#!/usr/bin/env python3
"""006 Minesweeper - test girdisi uretici.
Her test OYUNU BITIRIR (driver EOF'ta sonsuz donguye girdigi icin sart).
Senaryolar: win_full (hepsini ziyaret), win_mark (mayinlari isaretle+ziyaret),
            win_auto (isaretle+autoexplore+ziyaret), lose_v (mayina bas), lose_m (mayin-olmayani isaretle).
Beklenen cikti AYRICA referans server ile uretilir (bu script sadece .in yazar).
Uretir: ref/<name>.in
"""
import random, os

random.seed(6006)
HERE = os.path.dirname(os.path.abspath(__file__))


def gen_map(n, m, dens):
    for _ in range(200):
        grid = [['X' if random.random() < dens else '.' for _ in range(m)] for _ in range(n)]
        mines = [(i, j) for i in range(n) for j in range(m) if grid[i][j] == 'X']
        non = [(i, j) for i in range(n) for j in range(m) if grid[i][j] == '.']
        if mines and non:
            return grid, mines, non
    grid[0][0] = '.'
    return grid, mines, [(0, 0)] + non


CONFIGS = [(3, 3, 0.2), (4, 4, 0.2), (5, 5, 0.15), (4, 5, 0.25),
           (6, 6, 0.15), (3, 4, 0.3), (5, 6, 0.2), (7, 7, 0.12)]
SCEN = ["win_full", "win_mark", "win_auto", "lose_v", "lose_m"]

idx = 0
for (n, m, dens) in CONFIGS:
    for scen in SCEN:
        grid, mines, non = gen_map(n, m, dens)
        ops = []
        if scen == "win_full":
            ops = [(i, j, 0) for i, j in non]
        elif scen == "win_mark":
            ops = [(i, j, 1) for i, j in mines] + [(i, j, 0) for i, j in non]
        elif scen == "win_auto":
            seed = non[0]
            ops = [(seed[0], seed[1], 0)] + [(i, j, 1) for i, j in mines]
            for _ in range(n + m):
                for i, j in non:
                    ops.append((i, j, 2))
            ops += [(i, j, 0) for i, j in non]   # emniyet: kalanları ziyaret et
        elif scen == "lose_v":
            ops = [(mines[0][0], mines[0][1], 0)]           # mayina bas -> kaybet
        elif scen == "lose_m":
            ops = [(non[0][0], non[0][1], 1)]               # mayin-olmayani isaretle -> kaybet
        name = f"t{idx:02d}_{scen}"
        idx += 1
        with open(os.path.join(HERE, name + ".in"), "w", newline="\n") as f:
            f.write(f"{n} {m}\n")
            for row in grid:
                f.write("".join(row) + "\n")
            for (x, y, t) in ops:
                f.write(f"{x} {y} {t}\n")

print(f"{idx} test girdisi uretildi -> ref/*.in")
