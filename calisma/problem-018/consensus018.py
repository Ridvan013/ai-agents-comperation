#!/usr/bin/env python3
"""
018 Scheme - CONSENSUS oracle uretici.
Referans olmadigi icin: 4 aracin ciktisinda COGUNLUK = dogru kabul.
Her testte >=strict-cogunluk ayni ciktiyi verirse -> ref/<t>.out yazilir (o test 'karar verilebilir').
2-2 / hepsi farkli -> ref/<t>.out silinir (ambiguous, puanlamaya girmez).

Calistir: python3 consensus018.py   (agentlar bittikten SONRA; her aracı derler)
Sonra: python3 analiz.py            (her araci consensus'a gore puanlar)
"""
import os, subprocess, glob, collections, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
SONUC = os.path.join(HERE, "sonuclar")
AGENTS = ["claude-code", "cursor", "codex", "antigravity"]


def run_scheme(exe, infile, timeout=5):
    inp = open(infile, encoding="utf-8", errors="replace").read()
    full = inp.replace("\r\n", "\n").rstrip("\n") + "\n(exit)\n"
    try:
        r = subprocess.run([exe], input=full, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None
    lines = r.stdout.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    if lines:
        lines = lines[:-1]
    return "\n".join(ln.replace("scm> ", "") for ln in lines).rstrip("\n")


# 1) her araci derle
codes = {}
for a in AGENTS:
    sol = os.path.join(SONUC, a)
    if not os.path.isfile(os.path.join(sol, "src", "evaluation.cpp")):
        codes[a] = None; print(f"{a}: kaynak yok"); continue
    subprocess.run("cmake -B build >/dev/null 2>&1 && cmake --build build -j4 >/dev/null 2>&1",
                   shell=True, cwd=sol)
    code = os.path.join(sol, "build", "code")
    codes[a] = code if os.path.isfile(code) else None
    print(f"{a}: {'build OK' if codes[a] else 'DERLENMEDI'}")

tests = sorted((os.path.basename(p)[:-3] for p in glob.glob(os.path.join(REF, "*.in"))),
               key=lambda x: int(x) if x.isdigit() else 1 << 30)

# 2) her test: 4 aracin ciktisi -> cogunluk
karar = ambiguous = 0
for t in tests:
    outs = []
    for a in AGENTS:
        if codes[a]:
            g = run_scheme(codes[a], os.path.join(REF, t + ".in"))
            if g is not None:
                outs.append(g)
    outp = os.path.join(REF, t + ".out")
    if len(outs) >= 2:
        top, cnt = collections.Counter(outs).most_common(1)[0]
        if cnt > len(outs) / 2 and cnt >= 2:      # strict cogunluk
            open(outp, "w", encoding="utf-8", newline="\n").write(top + "\n")
            karar += 1
            continue
    # ambiguous -> varsa sil
    if os.path.isfile(outp):
        os.remove(outp)
    ambiguous += 1

print(f"\nConsensus: {karar}/{len(tests)} test karar verilebilir (ref/*.out yazildi), "
      f"{ambiguous} ambiguous (haric).")
