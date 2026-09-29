#!/usr/bin/env python3
"""
020 Buddy Algorithm - ozel dogruluk.
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code; sadece sol_dir icin kullanilir)

Yontem (canonical harness ile - agent kurcalayamaz):
  - Gecici dizinde: canonical ref/main.c + ref/buddy.h + ref/utils.h + AGENT'in buddy.c'si
  - gcc ile derle -> calistir
  - main.c KENDI KENDINI assert eder (ACMOJ 1848 resmi harness'i):
      her Phase'de ok()/dotOk() ile kontrol; hata olursa 'Assertion failed' + exit(-1).
      hepsi gecerse 'Test Ends.' basar.
  - Skor = gecen Phase sayisi (toplam 9 Phase: 1,2,3,4,5,6,7,8A,8B).
      'Test Ends.' varsa 9/9; degilse (yazilan Phase basligi sayisi - 1) [son phase'de patladi].

Cikti (analiz.py'nin parse ettigi): son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, re, tempfile, shutil
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

L = olcum.Limit("020")
lim_ok = False

exe = os.path.abspath(sys.argv[1])
SOL = os.path.dirname(exe)               # agent klasoru
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")          # canonical main.c + buddy.h + utils.h

TOPLAM_PHASE = 9                          # Phase 1,2,3,4,5,6,7,8A,8B (main.c sabit)

agent_buddy = os.path.join(SOL, "buddy.c")
if not os.path.isfile(agent_buddy):
    print("HATA: agent buddy.c yok")
    print(f"SONUC gecen=0 toplam={TOPLAM_PHASE} ms=0 rss_kb=0")
    sys.exit(0)

gecen = 0
ms = 0.0
rss = 0
leak_byte = -1

with tempfile.TemporaryDirectory() as td:
    for f in ("main.c", "buddy.h", "utils.h"):
        shutil.copy(os.path.join(REF, f), td)
    shutil.copy(agent_buddy, td)
    binp = os.path.join(td, "code")
    comp = subprocess.run(["gcc", "-O2", "-o", binp, os.path.join(td, "main.c"), os.path.join(td, "buddy.c")],
                          capture_output=True, text=True, timeout=120)
    if comp.returncode != 0 or not os.path.isfile(binp):
        son = comp.stderr.strip().splitlines()
        print(f"DERLEME HATASI ({son[-1][:90] if son else '?'})")
        print(f"SONUC gecen=0 toplam={TOPLAM_PHASE} ms=0 rss_kb=0")
        sys.exit(0)
    try:
        t0 = time.monotonic()
        r = olcum.run([binp], timeout=30)
        ms = (time.monotonic() - t0) * 1000
        out = r.stdout
        rss = r.rss_kb or 0
        lim_ok = L.ok(r)   # tek kosu = tek test case
    except subprocess.TimeoutExpired:
        print("TLE (30sn)")
        L.ok(None)
        print(L.satir(0, TOPLAM_PHASE))
        print(f"SONUC gecen=0 toplam={TOPLAM_PHASE} ms=30000 rss_kb=0")
        sys.exit(0)

    basliklar = len(re.findall(r"(?m)^Phase ", out))
    basarili_bitti = ("Test Ends." in out) and ("Assertion failed" not in out) and ("FAILED" not in out)
    if basarili_bitti:
        gecen = TOPLAM_PHASE
    else:
        gecen = max(0, basliklar - 1)     # son yazilan Phase'de patladi
    # ozet satirlari (Total: N Ok) detay olarak
    for ln in out.splitlines():
        if ln.startswith("Phase ") or ln.startswith("Total:") or ln.startswith("Test Ends") \
           or "Assertion failed" in ln or "FAILED" in ln:
            print(ln[:100])

    # --- LEAK: buddy testini valgrind ile (128MB+32768 op -> yavas, timeout 180; TLE olursa '-') ---
    if shutil.which("valgrind"):
        try:
            vr = subprocess.run(["valgrind", "--leak-check=full", "--error-exitcode=0", binp],
                                capture_output=True, text=True, timeout=180)
            mm = re.search(r"definitely lost:\s*([\d,]+)\s*bytes", vr.stderr)
            leak_byte = int(mm.group(1).replace(",", "")) if mm else 0
        except subprocess.TimeoutExpired:
            leak_byte = -1

print(L.satir(gecen if lim_ok else 0, TOPLAM_PHASE))
print(f"SONUC gecen={gecen} toplam={TOPLAM_PHASE} ms={ms:.0f} rss_kb={rss} leak_byte={leak_byte}")
