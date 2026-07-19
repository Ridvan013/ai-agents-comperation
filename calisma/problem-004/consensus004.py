#!/usr/bin/env python3
"""
004 Bookstore - CONSENSUS oracle uretici (referans yok).
4 aracin ciktisinda COGUNLUK = dogru kabul. Her testte strict-cogunluk ayni ciktiyi
verirse -> ref/<t>.out yazilir; 2-2/hepsi-farkli -> silinir (ambiguous, haric).

Calistir: python3 consensus004.py   (agentlar bittikten SONRA; her araci g++ ile derler)
Sonra: python3 analiz.py
"""
import os, subprocess, glob, collections, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
SONUC = os.path.join(HERE, "sonuclar")
AGENTS = ["claude-code", "cursor", "codex", "antigravity"]
JUNK = {"CMakeFiles", "build", ".git", "ref"}


def sources(sol):
    s = []
    for ext in ("*.cpp", "*.cc", "*.c"):
        s += glob.glob(os.path.join(sol, "**", ext), recursive=True)
    return [f for f in s if not any(j in f.replace("\\", "/").split("/") for j in JUNK)]


def build(sol):
    srcs = sources(sol)
    if not srcs:
        return None
    binp = f"/tmp/bs_{os.path.basename(sol)}"
    r = subprocess.run(["g++", "-O2", "-std=c++17", "-o", binp] + srcs,
                       capture_output=True, text=True)
    return binp if (r.returncode == 0 and os.path.isfile(binp)) else None


def run(binp, infile, timeout=10):
    inp = open(infile, encoding="utf-8", errors="replace").read()
    with tempfile.TemporaryDirectory() as wd:
        try:
            r = subprocess.run([binp], input=inp, capture_output=True, text=True,
                               cwd=wd, timeout=timeout)
        except subprocess.TimeoutExpired:
            return None
    return r.stdout.replace("\r\n", "\n").rstrip("\n")


codes = {}
for a in AGENTS:
    codes[a] = build(os.path.join(SONUC, a))
    print(f"{a}: {'build OK' if codes[a] else 'DERLENMEDI'}")

tests = sorted((os.path.basename(p)[:-3] for p in glob.glob(os.path.join(REF, "*.in"))),
               key=lambda x: int(x) if x.isdigit() else 1 << 30)

karar = ambiguous = 0
for t in tests:
    outs = []
    for a in AGENTS:
        if codes[a]:
            g = run(codes[a], os.path.join(REF, t + ".in"))
            if g is not None:
                outs.append(g)
    outp = os.path.join(REF, t + ".out")
    if len(outs) >= 2:
        top, cnt = collections.Counter(outs).most_common(1)[0]
        if cnt > len(outs) / 2 and cnt >= 2:
            open(outp, "w", encoding="utf-8", newline="\n").write(top + "\n")
            karar += 1
            continue
    if os.path.isfile(outp):
        os.remove(outp)
    ambiguous += 1

print(f"\nConsensus: {karar}/{len(tests)} karar verilebilir, {ambiguous} ambiguous (haric).")
