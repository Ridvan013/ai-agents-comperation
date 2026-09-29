#!/usr/bin/env python3
"""
013 STLite Map - ozel dogruluk (009 ile ayni mekanik).
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code; sadece sol_dir icin)

Yontem (canonical ref_data ile - agent kurcalayamaz):
  ref_data icindeki her test klasoru (code.cpp + answer.txt olan):
    - code.cpp'yi AGENT'in src/map.hpp'siyle derle  (-I <sol>/src  -I ref_data)
    - calistir, stdout'u answer.txt ile kiyasla (satir-sonu normalize)
  PASS = cikti esit.

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re, tempfile, shutil
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

L = olcum.Limit("013")

exe = os.path.abspath(sys.argv[1])
SOL = os.path.dirname(exe)
SRC = os.path.join(SOL, "src")
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref_data")


def norm(s):
    return s.replace("\r\n", "\n").rstrip("\n")


tests = sorted(d for d in os.listdir(REF)
               if os.path.isfile(os.path.join(REF, d, "code.cpp"))
               and os.path.isfile(os.path.join(REF, d, "answer.txt")))

gecen = gecen_lim = toplam = 0
max_ms = 0.0
max_rss = 0

for t in tests:
    code = os.path.join(REF, t, "code.cpp")
    ans = os.path.join(REF, t, "answer.txt")
    toplam += 1
    binp = f"/tmp/m013_{t.replace('.', '_')}"
    comp = subprocess.run(["g++", "-O2", "-std=c++17", "-I", SRC, "-I", REF, code, "-o", binp],
                          capture_output=True, text=True, timeout=120)
    if comp.returncode != 0:
        son = comp.stderr.strip().splitlines()
        print(f"{t}: DERLEME HATASI ({son[-1][:80] if son else '?'})")
        continue
    # bazi testler freopen("X.out","w",stdout) ile DOSYAYA yazar -> stdout bos kalir
    codetxt = open(code, encoding="utf-8", errors="replace").read()
    fm = re.search(r'freopen\(\s*"([^"]+)"\s*,\s*"w"\s*,\s*stdout', codetxt)
    tgt = fm.group(1) if fm else None
    try:
        t0 = time.monotonic()
        with tempfile.TemporaryDirectory() as wd:
            r = olcum.run([binp], cwd=wd, timeout=30)
            max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
            max_rss = max(max_rss, r.rss_kb or 0)
            if tgt and os.path.isfile(os.path.join(wd, tgt)):
                got = open(os.path.join(wd, tgt), encoding="utf-8", errors="replace").read()
            else:
                got = r.stdout
    except subprocess.TimeoutExpired:
        L.ok(None)
        print(f"{t}: TLE")
        continue
    lim_ok = L.ok(r)
    want = open(ans, encoding="utf-8", errors="replace").read()
    if norm(got) == norm(want):
        gecen += 1
        gecen_lim += int(lim_ok)
        print(f"{t}: PASS" + ("" if lim_ok else " (limit asildi)"))
    else:
        print(f"{t}: FAIL")

# --- LEAK: kucuk bir memcheck testini valgrind ile calistir (stres testi DEGIL, hizli) ---
leak_byte = -1
if shutil.which("valgrind") and tests:
    mc = sorted(t for t in tests if t.endswith(".memcheck"))
    lt = mc[0] if mc else sorted(tests)[0]
    code = os.path.join(REF, lt, "code.cpp")
    codetxt = open(code, encoding="utf-8", errors="replace").read()
    fm = re.search(r'freopen\(\s*"([^"]+)"\s*,\s*"w"\s*,\s*stdout', codetxt)
    tgt = fm.group(1) if fm else None
    binp = f"/tmp/m013_leak"
    comp = subprocess.run(["g++", "-O2", "-std=c++17", "-I", SRC, "-I", REF, code, "-o", binp],
                          capture_output=True, text=True, timeout=120)
    if comp.returncode == 0:
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
