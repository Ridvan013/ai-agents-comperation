#!/usr/bin/env python3
"""
006 Minesweeper (Basic/server) - ozel dogruluk.
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code = agent'in server binary'si)

Yontem (canonical ref/ ile):
  Her test (ref/<name>.in + <name>.out; .out referans server ile uretildi):
    - agent'in server'ini calistir: code < <name>.in   (TIMEOUT'lu! driver EOF'ta sonsuz donebilir)
    - stdout'u <name>.out ile kiyasla (satir-sonu normalize)
  PASS = cikti esit. TLE (sonsuz dongu / yavas) = FAIL.

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re, glob
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

exe = os.path.abspath(sys.argv[1])
SOL = os.path.dirname(exe)
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
L = olcum.Limit("006")


def norm(s):
    return s.replace("\r\n", "\n").rstrip("\n")


tests = sorted(os.path.basename(p)[:-3] for p in glob.glob(os.path.join(REF, "*.in"))
               if os.path.isfile(p[:-3] + ".out"))

if not os.path.isfile(exe):
    print("HATA: code (server) yok - derlenmedi mi?")
    print(f"SONUC gecen=0 toplam={len(tests)} ms=0 rss_kb=0")
    sys.exit(0)

gecen = gecen_lim = toplam = 0
max_ms = 0.0

for t in tests:
    fin = os.path.join(REF, t + ".in")
    fout = os.path.join(REF, t + ".out")
    toplam += 1
    try:
        t0 = time.monotonic()
        with open(fin, "rb") as f:
            r = olcum.run([exe], stdin=f, timeout=5)
        max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
    except subprocess.TimeoutExpired:
        L.ok(None)
        print(f"{t}: TLE (sonsuz dongu?)")
        continue
    lim_ok = L.ok(r)
    want = open(fout, encoding="utf-8", errors="replace").read()
    if norm(r.stdout) == norm(want):
        gecen += 1
        gecen_lim += int(lim_ok)
        print(f"{t}: PASS" + ("" if lim_ok else " (limit asildi)"))
    else:
        print(f"{t}: FAIL")

print(L.satir(gecen_lim, toplam))
print(f"SONUC gecen={gecen} toplam={toplam} ms={max_ms:.0f} rss_kb={L.max_rss_kb}")
