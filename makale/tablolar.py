#!/usr/bin/env python3
"""
Makaledeki tum sayisal tablolari tum-sonuclar.csv'den uretir (elle sayi kopyalama yok).
Kullanim: python makale/tablolar.py   -> ekrana: her tablo icin LaTeX satirlari + ozet sayilar

Toplama kurallari (Table: quality, danismanin tablosuyla ayni):
  Sec / G++ / Clang = 12 problemin TOPLAMI;  CCN / runtime / memory / binary = MEDYAN.
"""
import csv, os, statistics as st

KOK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROWS = list(csv.DictReader(open(os.path.join(KOK, "tum-sonuclar.csv"), encoding="utf-8")))
TOOLS = ["claude-code", "cursor", "codex", "antigravity"]
TNAME = {"claude-code": "Claude Code", "cursor": "Cursor", "codex": "Codex", "antigravity": "Antigravity"}
PROBS = ["001", "002", "004", "005", "006", "009", "013", "014", "017", "018", "019", "020"]
PNAME = {"001": "A+B", "002": "int2048", "004": "Bookstore", "005": "QOI codec", "006": "Minesweeper",
         "009": "vector", "013": "map", "014": "Python interp.", "017": "Train ticket",
         "018": "Scheme interp.", "019": "Attention sim.", "020": "Buddy alloc."}
TIER = {"001": "Low", "009": "Low", "020": "Low", "005": "Medium", "002": "Medium", "013": "Medium",
        "006": "High", "014": "High", "018": "High", "019": "High", "004": "High", "017": "High"}
# ProjDevBench'in problem basina yayimlanan ortalama skoru (kalibrasyon tablosu)
PDB = {"001": 54.4, "002": 48.2, "004": 36.3, "005": 58.9, "006": 53.5, "009": 58.5, "013": 58.2,
       "014": 46.2, "017": 53.2, "018": 32.9, "019": 36.9, "020": 33.3}

R = {(r["problem"], r["arac"]): r for r in ROWS}
missing = [(p, t) for p in PROBS for t in TOOLS if (p, t) not in R]
assert not missing, f"eksik kosu: {missing}"


def f(x):
    try: return float(x)
    except (TypeError, ValueError): return None


def ce(p, t):
    """derleme hatasi mi? (binary yok)"""
    return R[p, t]["binary_kb"] in ("-", "") and f(R[p, t]["exec_score"]) == 0


def fmt(x):
    """100.0 -> '100', 91.67 -> '91.7', None -> '--'"""
    if x is None: return "--"
    return f"{x:.0f}" if float(x).is_integer() else f"{x:.1f}"


def S_of(E, C):
    return 0.8 * E + 0.2 * C


print("% ===== Table: E / C / S (+ E_lim) per problem =====")
for p in PROBS:
    cells = []
    for t in TOOLS:
        r = R[p, t]
        if ce(p, t):
            cells.append(r"\textsc{ce} & -- & 0.0")
        else:
            cells.append(f"{fmt(f(r["exec_score"]))} & {fmt(f(r["cr_score"]))} & {f(r['birlesik']):.1f}")
    print(f"{p} {PNAME[p]}\n& " + "\n& ".join(cells) + r" \\" + "\n")

print("% means over 12 problems")
for col, lab in (("exec_score", "Mean E"), ("exec_lim", "Mean E_lim"), ("cr_score", "Mean C"),
                 ("birlesik", "Mean S")):
    vals = []
    for t in TOOLS:
        v = [f(R[p, t][col]) if f(R[p, t][col]) is not None else 0.0 for p in PROBS]
        vals.append(st.mean(v))
    print(lab, " | ".join(f"{TNAME[t]} {v:.1f}" for t, v in zip(TOOLS, vals)))

print("\n% ===== E vs E_lim per problem (official limits) =====")
for p in PROBS:
    line = []
    for t in TOOLS:
        r = R[p, t]
        e, el = f(r["exec_score"]), f(r["exec_lim"])
        extra = ""
        if r.get("tle") not in ("-", "", "0", None) or r.get("mle") not in ("-", "", "0", None):
            extra = f" [TLE {r.get('tle')} MLE {r.get('mle')}]"
        line.append(f"{TNAME[t]} E={fmt(e)} Elim={fmt(el)} cpu={r.get('cpu_ms')}ms rss={r.get('max_mb')}MB{extra}")
    print(p, " || ".join(line))

print("\n% ===== Table: quality (sum Sec/G++/Clang, median CCN/CPU/binary, limit violations) =====")
for t in TOOLS:
    def col(c):
        return [f(R[p, t][c]) for p in PROBS if f(R[p, t][c]) is not None]
    tle = sum(int(R[p, t]["tle"]) for p in PROBS if R[p, t]["tle"] not in ("-", ""))
    mle = sum(int(R[p, t]["mle"]) for p in PROBS if R[p, t]["mle"] not in ("-", ""))
    viol = sum(1 for p in PROBS if f(R[p, t]["exec_lim"]) is not None and f(R[p, t]["exec_score"]) is not None
               and f(R[p, t]["exec_lim"]) < f(R[p, t]["exec_score"]))
    print(f"{TNAME[t]:12s} & {sum(col('guvenlik')):.0f} & {sum(col('uyari')):.0f} & {sum(col('clang_tidy')):.0f}"
          f" & {st.median(col('max_ccn')):.1f} & {st.median(col('cpu_ms')):.0f} & {st.median(col('binary_kb')):.0f}"
          f" & {viol} \\\\   % median peak MB {st.median(col('max_mb')):.1f}; TLE runs {tle}, MLE runs {mle}")

print("\n% ===== Table: difficulty (mean E and E_lim by our complexity tier) =====")
for tier in ("Low", "Medium", "High"):
    ps = [p for p in PROBS if TIER[p] == tier]
    e = [st.mean(f(R[p, t]["exec_score"]) or 0 for p in ps) for t in TOOLS]
    el = [st.mean(f(R[p, t]["exec_lim"]) or 0 for p in ps) for t in TOOLS]
    print(f"{tier:6s} E    & " + " & ".join(f"{x:.1f}" for x in e) + r" \\")
    print(f"{tier:6s} Elim & " + " & ".join(f"{x:.1f}" for x in el) + r" \\")

print("\n% ===== Table: calibration (mean S over 4 tools vs ProjDevBench) =====")
gaps, ours_all, ours_lim_all = [], [], []
for p in PROBS:
    ours = st.mean(f(R[p, t]["birlesik"]) for t in TOOLS)
    ours_lim = st.mean(S_of(f(R[p, t]["exec_lim"]) or 0, f(R[p, t]["cr_score"]) or 0) for t in TOOLS)
    gaps.append(ours - PDB[p]); ours_all.append(ours); ours_lim_all.append(ours_lim)
    print(f"{p} {PNAME[p]:15s} & {PDB[p]:.1f} & {ours:5.1f} & $+{ours - PDB[p]:.1f}$ & {ours_lim:5.1f} & $+{ours_lim - PDB[p]:.1f}$ \\\\")
print(f"Mean & {st.mean(PDB.values()):.1f} & {st.mean(ours_all):.1f} & +{st.mean(gaps):.1f}"
      f" & {st.mean(ours_lim_all):.1f} & +{st.mean(ours_lim_all) - st.mean(PDB.values()):.1f}")
print("weakest tool mean S:", min(st.mean(f(R[p, t]["birlesik"]) for p in PROBS) for t in TOOLS))

print("\n% ===== Figure data (S and S_lim per problem, order", PROBS, ") =====")
for t in TOOLS:
    print(TNAME[t], "S    ", [f(R[p, t]["birlesik"]) for p in PROBS])
    print(TNAME[t], "S_lim", [round(S_of(f(R[p, t]["exec_lim"]) or 0, f(R[p, t]["cr_score"]) or 0), 1) for p in PROBS])
