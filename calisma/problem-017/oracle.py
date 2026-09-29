#!/usr/bin/env python3
"""
017 Train Ticket - ozel dogruluk (GERCEK ground-truth: out.txt).
Kullanim: python3 oracle.py <exe_path>   (exe = sol_dir/code)

Yontem (canonical ref/, agent kurcalayamaz):
  Her grup (basic_3..basic_6):
    - TAZE gecici dizin (disk state izolasyonu)
    - parcalari SIRAYLA calistir: code < 1.in, sonra < 2.in ... (program kapanip acilir;
      veri diskte kalmali -> kalicilik testi)
    - tum ciktilari birlestir, ref/<grup>/out.txt ile SATIR SATIR kiyasla
  Skor: gecen = eslesen satir sayisi, toplam = beklenen satir sayisi (graded acc).

Cikti: son satir 'SONUC gecen=X toplam=N ms=M rss_kb=R'
"""
import sys, os, subprocess, time, glob, tempfile, re
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import olcum

exe = os.path.abspath(sys.argv[1])
HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, "ref")
L = olcum.Limit("017")   # her parca (k.in) ayri bir test case

gruplar = sorted(d for d in glob.glob(os.path.join(REF, "basic_*"))
                 if os.path.isfile(os.path.join(d, "out.txt")))

toplam_beklenen = 0
for g in gruplar:
    toplam_beklenen += len(open(os.path.join(g, "out.txt"), encoding="utf-8",
                               errors="replace").read().rstrip("\n").split("\n"))

if not os.path.isfile(exe):
    print("HATA: code yok - derlenmedi mi?")
    print(f"SONUC gecen=0 toplam={toplam_beklenen} ms=0 rss_kb=0")
    sys.exit(0)

gecen = gecen_lim = 0
max_ms = 0.0

for g in gruplar:
    ad = os.path.basename(g)
    parcalar = sorted(glob.glob(os.path.join(g, "*.in")),
                      key=lambda p: int(re.sub(r"\D", "", os.path.basename(p)) or 0))
    want = open(os.path.join(g, "out.txt"), encoding="utf-8", errors="replace").read()
    want_lines = want.replace("\r\n", "\n").rstrip("\n").split("\n")
    got_parts = []
    part_ok = []      # parca limitler icinde mi (resmi TL/ML)
    tle = False
    with tempfile.TemporaryDirectory() as wd:
        for p in parcalar:
            inp = open(p, encoding="utf-8", errors="replace").read()
            try:
                t0 = time.monotonic()
                r = olcum.run([exe], input=inp, cwd=wd, timeout=120)
                max_ms = max(max_ms, (time.monotonic() - t0) * 1000)
            except subprocess.TimeoutExpired:
                L.ok(None)
                tle = True
                break
            got_parts.append(r.stdout)
            part_ok.append(L.ok(r))
    if tle:
        print(f"{ad}: TLE (120sn)")
        continue
    got_all = "".join(got_parts).replace("\r\n", "\n").rstrip("\n")
    got_lines = got_all.split("\n")
    # her ciktinin satirinin hangi parcadan geldigi -> limit asan parcanin satirlari sayilmaz
    kaynak = []
    for k, part in enumerate(got_parts):
        part = part.replace("\r\n", "\n")
        kaynak += [k] * (part.count("\n") + (1 if part and not part.endswith("\n") else 0))
    eslesen = eslesen_lim = 0
    for i, w in enumerate(want_lines):
        if i < len(got_lines) and got_lines[i] == w:
            eslesen += 1
            if i < len(kaynak) and part_ok[kaynak[i]]:
                eslesen_lim += 1
    gecen += eslesen
    gecen_lim += eslesen_lim
    durum = "PASS" if eslesen == len(want_lines) and len(got_lines) == len(want_lines) else "FAIL"
    asan = sum(1 for ok in part_ok if not ok)
    print(f"{ad}: {durum}  ({eslesen}/{len(want_lines)} satir dogru, %{eslesen/len(want_lines)*100:.1f})"
          + (f" · {asan}/{len(part_ok)} parca limit asti -> limitli {eslesen_lim}" if asan else ""))

print(L.satir(gecen_lim, toplam_beklenen))
print(f"SONUC gecen={gecen} toplam={toplam_beklenen} ms={max_ms:.0f} rss_kb={L.max_rss_kb}")
