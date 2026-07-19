#!/usr/bin/env python3
"""
015 File Storage - ozel dogruluk (GERCEK ground-truth: Python multimap).
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code)

Yontem (canonical ref/):
  - agent'in programi TAZE dizinde calisir: code < ref/input.txt  (disk dosyalari izole)
  - stdout'taki find satirlari ref/expected.txt (Python multimap) ile SATIR SATIR kiyaslanir.
  Skor: gecen = eslesen find satiri, toplam = beklenen find sayisi (graded).

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, tempfile, re

exe = os.path.abspath(sys.argv[1])
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
INPUT = os.path.join(REF, "input.txt")
EXP = os.path.join(REF, "expected.txt")

exp = open(EXP, encoding="utf-8", errors="replace").read().replace("\r\n", "\n").rstrip("\n").split("\n")
T = len(exp)

if not os.path.isfile(exe):
    print("HATA: code yok - derlenmedi mi?")
    print(f"SONUC gecen=0 toplam={T} ms=0 rss_kb=0")
    sys.exit(0)

ms = 0.0
rss = 0
inp = open(INPUT, "rb").read()
try:
    with tempfile.TemporaryDirectory() as wd:
        t0 = time.monotonic()
        r = subprocess.run(["/usr/bin/time", "-v", exe], input=inp,
                           capture_output=True, cwd=wd, timeout=120)
        ms = (time.monotonic() - t0) * 1000
        m = re.search(rb"Maximum resident set size \(kbytes\):\s*(\d+)", r.stderr)
        if m:
            rss = int(m.group(1))
        got = r.stdout.decode("utf-8", "replace").replace("\r\n", "\n").rstrip("\n").split("\n")
except subprocess.TimeoutExpired:
    print("TLE (120sn)")
    print(f"SONUC gecen=0 toplam={T} ms=120000 rss_kb=0")
    sys.exit(0)

gecen = sum(1 for i in range(T) if i < len(got) and got[i] == exp[i])
ilk = next((i + 1 for i in range(T) if i >= len(got) or got[i] != exp[i]), None)
if ilk:
    g = got[ilk - 1] if ilk - 1 < len(got) else "<yok>"
    print(f"ilk uyusmazlik: find #{ilk}  beklenen='{exp[ilk-1][:30]}'  gelen='{g[:30]}'")
print(f"gecen {gecen}/{T} find dogru (%{gecen/T*100:.1f})")
print(f"SONUC gecen={gecen} toplam={T} ms={ms:.0f} rss_kb={rss}")
