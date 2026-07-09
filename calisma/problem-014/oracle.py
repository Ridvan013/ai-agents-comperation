#!/usr/bin/env python3
"""
014 Python Interpreter - ozel dogruluk (differential vs python3).
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code)

Yontem (canonical ref/ ile):
  Her test (ref/<name>.in + <name>.out; .out python3+spec-preamble ile uretildi):
    - agent'in 'code' interpreter'ini calistir: code < <name>.in
    - stdout'u <name>.out ile kiyasla (satir-sonu normalize)
  PASS = cikti esit.

HARIC testler (python3 simplified-Python'dan ayrisiyor): test13 (scoping), test14 (f-string float).

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re, glob

exe = os.path.abspath(sys.argv[1])
SOL = os.path.dirname(exe)
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")


def norm(s):
    return s.replace("\r\n", "\n").rstrip("\n")


tests = sorted(os.path.basename(p)[:-3] for p in glob.glob(os.path.join(REF, "*.in"))
               if os.path.isfile(p[:-3] + ".out"))

if not os.path.isfile(exe):
    print("HATA: code (interpreter) yok - derlenmedi mi?")
    print(f"SONUC gecen=0 toplam={len(tests)} ms=0 rss_kb=0")
    sys.exit(0)

gecen = toplam = 0
max_ms = 0.0
max_rss = 0

for t in tests:
    fin = os.path.join(REF, t + ".in")
    fout = os.path.join(REF, t + ".out")
    toplam += 1
    try:
        t0 = time.monotonic()
        with open(fin, "rb") as f:
            r = subprocess.run(["/usr/bin/time", "-v", exe], stdin=f,
                               capture_output=True, text=True, timeout=15)
        max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
        m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", r.stderr)
        if m:
            max_rss = max(max_rss, int(m.group(1)))
    except subprocess.TimeoutExpired:
        print(f"{t}: TLE")
        continue
    want = open(fout, encoding="utf-8", errors="replace").read()
    if norm(r.stdout) == norm(want):
        gecen += 1
        print(f"{t}: PASS")
    else:
        print(f"{t}: FAIL")

print(f"SONUC gecen={gecen} toplam={toplam} ms={max_ms:.0f} rss_kb={max_rss}")
