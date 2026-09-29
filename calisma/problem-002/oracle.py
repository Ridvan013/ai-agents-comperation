#!/usr/bin/env python3
"""
002 int2048 - ozel dogruluk (differential vs Python bigint).
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code; sadece sol_dir icin)

Yontem (canonical, agent kurcalayamaz):
  - Agent'in src/int2048.cpp + src/include/int2048.h'i, canonical ref/driver.cpp ile derlenir.
  - ref/input.txt (352 islem) calistirilir, cikti ref/expected.txt (Python bigint) ile
    SATIR SATIR kiyaslanir. gecen = eslesen satir sayisi.
  - Python'un // floor bolme ve floored mod'u spec ile ayni.

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re, tempfile, shutil
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

L = olcum.Limit("002")
exe = os.path.abspath(sys.argv[1])
SOL = os.path.dirname(exe)
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
DRIVER = os.path.join(REF, "driver.cpp")
INPUT = os.path.join(REF, "input.txt")
EXPECTED = os.path.join(REF, "expected.txt")

inc = os.path.join(SOL, "src", "include")
srccpp = os.path.join(SOL, "src", "int2048.cpp")


def norm(lines):
    return [ln.rstrip("\r") for ln in lines]


exp = norm(open(EXPECTED, encoding="utf-8", errors="replace").read().split("\n"))
while exp and exp[-1] == "":
    exp.pop()
toplam = len(exp)

if not os.path.isfile(srccpp):
    print("HATA: agent src/int2048.cpp yok")
    print(f"SONUC gecen=0 toplam={toplam} ms=0 rss_kb=0")
    sys.exit(0)

binp = tempfile.NamedTemporaryFile(delete=False, suffix="_c002").name
comp = subprocess.run(["g++", "-O2", "-std=c++17", "-I", inc, DRIVER, srccpp, "-o", binp],
                      capture_output=True, text=True, timeout=180)
if comp.returncode != 0 or not os.path.isfile(binp):
    son = comp.stderr.strip().splitlines()
    print(f"DERLEME HATASI ({son[-1][:100] if son else '?'})")
    print(f"SONUC gecen=0 toplam={toplam} ms=0 rss_kb=0")
    sys.exit(0)

ms = 0.0
rss = 0
try:
    t0 = time.monotonic()
    with open(INPUT, "rb") as fin:
        r = olcum.run([binp], stdin=fin, timeout=60)
    ms = (time.monotonic() - t0) * 1000
    rss = r.rss_kb or 0
    lim_ok = L.ok(r)   # tek kosu = tek test case
    got = norm(r.stdout.split("\n"))
    while got and got[-1] == "":
        got.pop()
except subprocess.TimeoutExpired:
    print("TLE (60sn)")
    L.ok(None)
    print(L.satir(0, toplam))
    print(f"SONUC gecen=0 toplam={toplam} ms=60000 rss_kb=0")
    os.unlink(binp)
    sys.exit(0)

gecen = 0
ilk_hata = None
for i in range(toplam):
    g = got[i] if i < len(got) else "<yok>"
    if g == exp[i]:
        gecen += 1
    elif ilk_hata is None:
        ilk_hata = (i + 1, exp[i][:30], g[:30])

if ilk_hata:
    print(f"ilk uyusmazlik: satir {ilk_hata[0]}  beklenen={ilk_hata[1]}  gelen={ilk_hata[2]}")
print(f"gecen {gecen}/{toplam} islem dogru")

# --- LEAK: kucuk girdiyle (ilk 40 islem) valgrind ---
leak_byte = -1
if shutil.which("valgrind") and os.path.isfile(binp):
    kucuk = "\n".join(open(INPUT, encoding="utf-8", errors="replace").read().split("\n")[:40]) + "\n"
    try:
        vr = subprocess.run(["valgrind", "--leak-check=full", "--error-exitcode=0", binp],
                            input=kucuk, capture_output=True, text=True, timeout=180)
        mm = re.search(r"definitely lost:\s*([\d,]+)\s*bytes", vr.stderr)
        leak_byte = int(mm.group(1).replace(",", "")) if mm else 0
    except subprocess.TimeoutExpired:
        leak_byte = -1

try:
    os.unlink(binp)
except Exception:
    pass
print(L.satir(gecen if lim_ok else 0, toplam))
print(f"SONUC gecen={gecen} toplam={toplam} ms={ms:.0f} rss_kb={rss} leak_byte={leak_byte}")
