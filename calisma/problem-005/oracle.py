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
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

exe = os.path.abspath(sys.argv[1])
HERE = os.path.dirname(os.path.abspath(__file__))
RESIM = os.path.join(HERE, "resim")
L = olcum.Limit("005")

SETS = [
    ("rgb",  "-3", "ppm", ["pic1", "pic2", "pic3", "testcard"]),
    ("rgba", "-4", "pam", ["dice", "kokona", "logo", "testcard"]),
]


def run(flag_de, girdi, td):
    """(stdout, limit_ok) - her cagri ayri bir test-case kosusudur."""
    r = olcum.run([exe] + flag_de, input=girdi, cwd=td, timeout=60, text=False)
    return r.stdout, L.ok(r)


gecen = gecen_lim = toplam = 0
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
                enc, enc_lim = run(["-e", flag], img_b, td)      # encode
                dec, dec_lim = run(["-d", flag], ref_b, td)      # decode ref qoi -> ppm
                reenc, re_lim = run(["-e", flag], dec, td)       # re-encode -> qoi'
                max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
                enc_ok = (enc == ref_b)
                dec_ok = (reenc == ref_b)
                gecen += int(enc_ok) + int(dec_ok)
                gecen_lim += int(enc_ok and enc_lim) + int(dec_ok and dec_lim and re_lim)
                print(f"{tur}/{name}: encode={'PASS' if enc_ok else 'FAIL'} · decode={'PASS' if dec_ok else 'FAIL'}")
            except subprocess.TimeoutExpired:
                L.ok(None)
                print(f"{tur}/{name}: TLE (encode+decode FAIL)")
            except Exception as e:
                print(f"{tur}/{name}: HATA ({e})")

# en yuksek RSS: sadece agent programi (/usr/bin/time ile, her kosu ayri)
rss_kb = L.max_rss_kb

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

print(L.satir(gecen_lim, toplam))
print(f"SONUC gecen={gecen} toplam={toplam} ms={max_ms:.0f} rss_kb={rss_kb} leak_byte={leak_byte}")
