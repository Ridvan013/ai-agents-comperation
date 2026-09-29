#!/usr/bin/env python3
"""
019 GPU Attention Simulator - ozel dogruluk.
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code; sadece sol_dir icin)

Yontem (canonical, agent kurcalayamaz):
  - canonical ref/main.cpp + ref/simulator.hpp + AGENT'in src.hpp'si derlenir.
  - ref/ dizininde (gercek data/ ans.txt ile) calistirilir.
  - RESMI Rater stderr'e 'Error Rate: X' basar (X = hatali eleman orani; 0 = kusursuz).
    Rater beklenen cevaplarla (ans.txt) kiyasladigi icin GERCEK ground-truth.
  - acc = max(0, 1-X). Skor: gecen = round(acc*10000), toplam=10000 (exec_score = acc*100).
  - 'Error Rate' hic cikmadiysa (cokme/eksik) -> 0.

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

L = olcum.Limit("019")

exe = os.path.abspath(sys.argv[1])
SOL = os.path.dirname(exe)
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
MAIN = os.path.join(REF, "main.cpp")

T = 10000

if not os.path.isfile(os.path.join(SOL, "src.hpp")):
    print("HATA: agent src.hpp yok")
    print(f"SONUC gecen=0 toplam={T} ms=0 rss_kb=0")
    sys.exit(0)

binp = "/tmp/code019_" + str(os.getpid())
comp = subprocess.run(["g++", "-O2", "-std=c++17", MAIN, "-I", SOL, "-I", REF, "-o", binp],
                      capture_output=True, text=True, timeout=180)
if comp.returncode != 0 or not os.path.isfile(binp):
    son = comp.stderr.strip().splitlines()
    print(f"DERLEME HATASI ({son[-1][:90] if son else '?'})")
    print(f"SONUC gecen=0 toplam={T} ms=0 rss_kb=0")
    sys.exit(0)

ms = 0.0
rss = 0
try:
    t0 = time.monotonic()
    r = olcum.run([binp], cwd=REF, timeout=120)
    ms = (time.monotonic() - t0) * 1000
    rss = r.rss_kb or 0
    lim_ok = L.ok(r)   # tek kosu = tek test case
    out = r.stdout + r.stderr
except subprocess.TimeoutExpired:
    print("TLE (120sn)")
    L.ok(None)
    print(L.satir(0, T))
    print(f"SONUC gecen=0 toplam={T} ms=120000 rss_kb=0")
    try: os.unlink(binp)
    except Exception: pass
    sys.exit(0)

mm = re.search(r"Error Rate:\s*([\d.]+)", out)
if mm:
    rate = float(mm.group(1))
    acc = max(0.0, 1.0 - rate)
    gecen = round(acc * T)
    print(f"Error Rate: {rate:.6f} -> acc %{acc*100:.2f}")
else:
    gecen = 0
    print("(Error Rate cikmadi -> cokme/eksik cozum -> 0)")

try: os.unlink(binp)
except Exception: pass
print(L.satir(gecen if lim_ok else 0, T))
print(f"SONUC gecen={gecen} toplam={T} ms={ms:.0f} rss_kb={rss}")
