#!/usr/bin/env python3
"""
004 Bookstore - ozel dogruluk (cross-agent consensus).
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code)

Beklenen ciktilar ref/<t>.out 'consensus004.py' ile 4 aracin cogunlugundan uretilir.
Bu oracle SADECE agent'i o consensus'a gore puanlar (consensus004.py once calismali).
Her test TAZE gecici dizinde calisir (dosya-kaliciligi izolasyonu).

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, glob, tempfile
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

exe = os.path.abspath(sys.argv[1])
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
L = olcum.Limit("004")


def norm(s):
    return s.replace("\r\n", "\n").rstrip("\n")


def run(exe, infile, timeout=10):
    """(stdout, limit_ok); zaman asiminda (None, False)."""
    inp = open(infile, encoding="utf-8", errors="replace").read()
    with tempfile.TemporaryDirectory() as wd:
        try:
            r = olcum.run([exe], input=inp, cwd=wd, timeout=timeout)
        except subprocess.TimeoutExpired:
            L.ok(None)
            return None, False
    return r.stdout, L.ok(r)


tests = sorted((os.path.basename(p)[:-3] for p in glob.glob(os.path.join(REF, "*.in"))
                if os.path.isfile(p[:-3] + ".out")),
               key=lambda x: int(x) if x.isdigit() else 1 << 30)

if not tests:
    print("(consensus henuz uretilmedi: once 'python3 consensus004.py' calistir)")
    print("SONUC gecen=0 toplam=0 ms=0 rss_kb=0")
    sys.exit(0)
if not os.path.isfile(exe):
    print("HATA: code yok - derlenmedi mi?")
    print(f"SONUC gecen=0 toplam={len(tests)} ms=0 rss_kb=0")
    sys.exit(0)

gecen = gecen_lim = toplam = 0
max_ms = 0.0
for t in tests:
    toplam += 1
    t0 = time.monotonic()
    got, lim_ok = run(exe, os.path.join(REF, t + ".in"))
    max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
    if got is None:
        continue
    want = open(os.path.join(REF, t + ".out"), encoding="utf-8", errors="replace").read()
    if norm(got) == norm(want):
        gecen += 1
        gecen_lim += int(lim_ok)

print(f"gecen {gecen}/{toplam} (consensus'a gore)")
print(L.satir(gecen_lim, toplam))
print(f"SONUC gecen={gecen} toplam={toplam} ms={max_ms:.0f} rss_kb={L.max_rss_kb}")
