#!/usr/bin/env python3
"""
005 QOI Codec - ozel dogruluk.
Kullanim: python3 oracle.py <exe_path>

Yontem (referans .qoi ile - deterministik, PPM header/comment sorunu yok):
  - ENCODE testi:  ppm/pam -> encode -> qoi.  referans .qoi ile byte-byte ayni mi?
  - DECODE testi:  referans .qoi -> decode -> ppm2 -> re-encode -> qoi'.  referans .qoi ile ayni mi?
  encode ve decode AYRI skorlanir (resim basina 2 alt-test). Boylece sadece
  encoder'i dogru olan (ornek: antigravity) yarim puan alir, 0 degil.

QOI encode determinisik (README'deki oncelik: RUN>INDEX>DIFF>LUMA>RGB/RGBA),
bu yuzden dogru kod referans .qoi'yi birebir uretir.

Cikti (analiz.py'nin parse ettigi): son satir 'SONUC gecen=X toplam=N ms=M'
"""
import sys, os, subprocess, tempfile, time, resource, shutil, re

exe = os.path.abspath(sys.argv[1])
HERE = os.path.dirname(os.path.abspath(__file__))
RESIM = os.path.join(HERE, "resim")

SETS = [
    ("rgb",  "-3", "ppm", ["pic1", "pic2", "pic3", "testcard"]),
    ("rgba", "-4", "pam", ["dice", "kokona", "logo", "testcard"]),
]


def run(flag_de, girdi, td):
    return subprocess.run([exe] + flag_de, input=girdi, capture_output=True, cwd=td, timeout=60).stdout


gecen = toplam = 0
max_ms = 0.0

for tur, flag, ext, imgs in SETS:
    for name in imgs:
        img = os.path.join(RESIM, tur, f"{name}.{ext}")
        refq = os.path.join(RESIM, tur, f"{name}.qoi")
        if not (os.path.isfile(img) and os.path.isfile(refq)):
            continue
        toplam += 2   # encode ve decode AYRI alt-test sayilir (resim basina 2)
        img_b = open(img, "rb").read()
        ref_b = open(refq, "rb").read()
        with tempfile.TemporaryDirectory() as td:
            try:
                t0 = time.monotonic()
                enc = run(["-e", flag], img_b, td)          # encode
                dec = run(["-d", flag], ref_b, td)          # decode ref qoi -> ppm
                reenc = run(["-e", flag], dec, td)          # re-encode -> qoi'
                max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
                enc_ok = (enc == ref_b)
                dec_ok = (reenc == ref_b)
                gecen += int(enc_ok) + int(dec_ok)
                print(f"{tur}/{name}: encode={'PASS' if enc_ok else 'FAIL'} · decode={'PASS' if dec_ok else 'FAIL'}")
            except subprocess.TimeoutExpired:
                print(f"{tur}/{name}: TLE (encode+decode FAIL)")
            except Exception as e:
                print(f"{tur}/{name}: HATA ({e})")

# cocuk process'lerin en yuksek RSS'i (KB, Linux) - bellek metrigi icin
rss_kb = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss

# --- LEAK: bir encode islemini (rgb/testcard, en kucuk) valgrind ile ---
leak_byte = -1
img = os.path.join(RESIM, "rgb", "testcard.ppm")
if shutil.which("valgrind") and os.path.isfile(exe) and os.path.isfile(img):
    try:
        with tempfile.TemporaryDirectory() as td:
            vr = subprocess.run(["valgrind", "--leak-check=full", "--error-exitcode=0", exe, "-e", "-3"],
                                input=open(img, "rb").read(), capture_output=True, cwd=td, timeout=180)
            mm = re.search(rb"definitely lost:\s*([\d,]+)\s*bytes", vr.stderr)
            leak_byte = int(mm.group(1).replace(b",", b"")) if mm else 0
    except subprocess.TimeoutExpired:
        leak_byte = -1

print(f"SONUC gecen={gecen} toplam={toplam} ms={max_ms:.0f} rss_kb={rss_kb} leak_byte={leak_byte}")
