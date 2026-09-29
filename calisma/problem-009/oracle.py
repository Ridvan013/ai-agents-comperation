#!/usr/bin/env python3
"""
009 STLite Vector - ozel dogruluk.
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code; sadece sol_dir icin kullanilir)

Yontem (canonical ref_data ile - agent kurcalayamaz):
  Her test (ref_data/one..seven):
    - code.cpp'yi AGENT'in src/vector.hpp'siyle derle  (-I <sol>/src  -I ref_data)
    - calistir, stdout'u ref_data/<t>/answer.txt ile kiyasla
  PASS = cikti birebir (satir-sonu normalize) esit.

Cikti (analiz.py'nin parse ettigi): son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re, shutil, tempfile
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

L = olcum.Limit("009")

exe = os.path.abspath(sys.argv[1])
SOL = os.path.dirname(exe)               # agent klasoru
SRC = os.path.join(SOL, "src")           # agent'in vector.hpp + exceptions/utility
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref_data")     # canonical code.cpp + answer.txt + class-*.hpp
TESTS = ["one", "two", "three", "four", "five", "six", "seven"]


def norm(s):
    return s.replace("\r\n", "\n").rstrip("\n")


gecen = gecen_lim = toplam = 0
max_ms = 0.0
max_rss = 0   # SADECE test binary'sinin RSS'i (g++ derleyici bellegi haric - /usr/bin/time ile)

for t in TESTS:
    code = os.path.join(REF, t, "code.cpp")
    ans = os.path.join(REF, t, "answer.txt")
    if not (os.path.isfile(code) and os.path.isfile(ans)):
        continue
    toplam += 1
    binp = f"/tmp/v009_{t}"
    comp = subprocess.run(["g++", "-O2", "-std=c++17", "-I", SRC, "-I", REF, code, "-o", binp],
                          capture_output=True, text=True, timeout=120)
    if comp.returncode != 0:
        son = comp.stderr.strip().splitlines()
        print(f"{t}: DERLEME HATASI ({son[-1][:80] if son else '?'})")
        continue
    try:
        t0 = time.monotonic()
        # /usr/bin/time: test binary'sinin kendi CPU/peak RSS'i (derleyici degil)
        r = olcum.run([binp], timeout=30)
        max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
        max_rss = max(max_rss, r.rss_kb or 0)
    except subprocess.TimeoutExpired:
        L.ok(None)
        print(f"{t}: TLE")
        continue
    lim_ok = L.ok(r)
    want = open(ans, encoding="utf-8", errors="replace").read()
    if norm(r.stdout) == norm(want):
        gecen += 1
        gecen_lim += int(lim_ok)
        print(f"{t}: PASS" + ("" if lim_ok else " (limit asildi)"))
    else:
        print(f"{t}: FAIL")

# --- LEAK: kucuk bir testi (one; stres testi four DEGIL) valgrind ile ---
leak_byte = -1
if shutil.which("valgrind"):
    binp = "/tmp/v009_one"
    if os.path.isfile(binp):
        try:
            with tempfile.TemporaryDirectory() as wd:
                vr = subprocess.run(["valgrind", "--leak-check=full", "--error-exitcode=0", binp],
                                    cwd=wd, capture_output=True, text=True, timeout=180)
                mm = re.search(r"definitely lost:\s*([\d,]+)\s*bytes", vr.stderr)
                leak_byte = int(mm.group(1).replace(",", "")) if mm else 0
        except subprocess.TimeoutExpired:
            leak_byte = -1

print(L.satir(gecen_lim, toplam))
print(f"SONUC gecen={gecen} toplam={toplam} ms={max_ms:.0f} rss_kb={max_rss} leak_byte={leak_byte}")
