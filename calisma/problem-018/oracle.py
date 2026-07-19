#!/usr/bin/env python3
"""
018 Scheme Interpreter - ozel dogruluk (cross-agent consensus).
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code)

ONEMLI: Referans yok. Beklenen ciktilar (ref/<t>.out) 'consensus018.py' ile
4 aracin cogunlugundan uretilir. Bu oracle SADECE agent'i o consensus'a gore puanlar.
consensus018.py calismadiysa ref/*.out yoktur -> 0/0 doner (once consensus018.py calistir).

REPL temizligi (score.sh ile ayni): input + '(exit)' besle, ciktinin SON satirini at,
her satirdan 'scm> ' prompt'unu cikar.

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re, glob

exe = os.path.abspath(sys.argv[1])
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")


def run_scheme(exe, infile, timeout=5):
    inp = open(infile, encoding="utf-8", errors="replace").read()
    full = inp.replace("\r\n", "\n").rstrip("\n") + "\n(exit)\n"
    try:
        r = subprocess.run([exe], input=full, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None, timeout * 1000
    lines = r.stdout.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    if lines:
        lines = lines[:-1]                       # score.sh: son satiri at
    cleaned = [ln.replace("scm> ", "") for ln in lines]
    return "\n".join(cleaned).rstrip("\n"), None


# consensus tarafindan uretilmis beklenen ciktilar
tests = sorted((os.path.basename(p)[:-3] for p in glob.glob(os.path.join(REF, "*.in"))
                if os.path.isfile(p[:-3] + ".out")),
               key=lambda x: int(x) if x.isdigit() else 1 << 30)

if not tests:
    print("(consensus henuz uretilmedi: once 'python3 consensus018.py' calistir)")
    print("SONUC gecen=0 toplam=0 ms=0 rss_kb=0")
    sys.exit(0)

if not os.path.isfile(exe):
    print("HATA: code yok - derlenmedi mi?")
    print(f"SONUC gecen=0 toplam={len(tests)} ms=0 rss_kb=0")
    sys.exit(0)

gecen = toplam = 0
max_ms = 0.0
for t in tests:
    toplam += 1
    t0 = time.monotonic()
    got, tle = run_scheme(exe, os.path.join(REF, t + ".in"))
    max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
    if got is None:
        continue
    want = open(os.path.join(REF, t + ".out"), encoding="utf-8", errors="replace").read().rstrip("\n")
    if got == want:
        gecen += 1

print(f"gecen {gecen}/{toplam} (consensus'a gore)")
print(f"SONUC gecen={gecen} toplam={toplam} ms={max_ms:.0f} rss_kb=0")
