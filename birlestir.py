#!/usr/bin/env python3
"""
Tum problemlerin sonuclar.csv'lerini tek dosyada birlestirir.
Kullanim: python3 birlestir.py
Cikti: staj1/tum-sonuclar.csv  (her satir = bir problem x arac)

Tarar: calisma/problem-*/sonuclar.csv  +  pilot/problem-*/sonuclar.csv
"""
import csv, glob, os

KOK = os.path.dirname(os.path.abspath(__file__))
CIKTI = os.path.join(KOK, "tum-sonuclar.csv")

# analiz.py ile ayni sutun sirasi (referans baslik)
kaynaklar = sorted(
    glob.glob(os.path.join(KOK, "calisma", "problem-*", "sonuclar.csv")) +
    glob.glob(os.path.join(KOK, "pilot", "problem-*", "sonuclar.csv"))
)

if not kaynaklar:
    print("Hic sonuclar.csv bulunamadi.")
    raise SystemExit(1)

baslik = None
satirlar = []
kaynak_say = 0

for yol in kaynaklar:
    with open(yol, newline="", encoding="utf-8") as f:
        r = list(csv.reader(f))
    if not r:
        continue
    h, data = r[0], r[1:]
    if baslik is None:
        baslik = h
    elif h != baslik:
        print(f"UYARI: baslik farkli, atlaniyor -> {os.path.relpath(yol, KOK)}")
        continue
    data = [row for row in data if any(c.strip() for c in row)]  # bos satir atla
    satirlar += data
    kaynak_say += 1
    rel = os.path.relpath(yol, KOK)
    print(f"  + {rel:45s} {len(data)} satir")

# problem numarasina gore artan sirala (001..020), esitse araca gore
def anahtar(row):
    prob = row[0] if row else ""
    arac = row[1] if len(row) > 1 else ""
    try:
        pnum = int(prob)
    except ValueError:
        pnum = 9999
    return (pnum, arac)

satirlar.sort(key=anahtar)

with open(CIKTI, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(baslik)
    w.writerows(satirlar)

print(f"\n{kaynak_say} dosya birlestirildi -> {os.path.relpath(CIKTI, KOK)}  ({len(satirlar)} toplam satir)")
