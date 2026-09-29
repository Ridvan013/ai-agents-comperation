#!/usr/bin/env python3
"""
Toplu analiz scripti — tum karsilastirma parametreleri.

Olcer (otomatik):
  - Dogruluk (pass@1) + verdict (PASS/WA/TLE/RE/MLE/CE) + her testin gercek ciktisi
  - Max bellek (RSS), calisma hizi (ms)
  - Derleyici uyarilari (-Wall -Wextra), binary boyutu
  - Kod kalitesi: avg/max cyclomatic complexity (lizard), yorum orani
  - Guvenlik: cppcheck (offline), clang-tidy
  - Bellek sizintisi: valgrind
  - Kendi testini yazdi mi
  - Commit, churn (tohum haric), satir
  - Code Review proxy + Birlesik skor (0.8*exec + 0.2*CR)
  + kayit.csv (manuel: model, tur, mudahale, not)

Cikti: ekrana + rapor.txt (detay) + sonuclar.csv (ozet/tam)
Linux/WSL'de: python3 analiz.py
"""
import os, sys, glob, shutil, subprocess, csv, re, time, json

HERE = os.path.dirname(os.path.abspath(__file__))
for _d in (os.path.dirname(HERE), os.path.join(os.path.dirname(os.path.dirname(HERE)), "calisma")):
    if os.path.isfile(os.path.join(_d, "olcum.py")):   # calisma/ veya pilot/ altindan
        sys.path.insert(0, _d); break
import olcum   # calisma/olcum.py: resmi TL/ML limitleri + temiz kopya
SONUC_DIR = os.path.join(HERE, "sonuclar")
TEST_DIR = os.path.join(HERE, "testler")
TIME_LIMIT = 5
MEM_LIMIT_MB = 256
ATLA = {"ornek-dogru", "ornek-hatali"}
SRC_EXT = (".cpp", ".c", ".h", ".hpp", ".cc")
BUILD_JUNK = {"CMakeCache.txt", "cmake_install.cmake", "Makefile", "code", "code.exe", "a.out",
              "derleme_uyari.log", "derleme_hata.log"}
SEED_FILES = {"README.md", "CMakeLists.txt"}
HAS_TIME = os.path.isfile("/usr/bin/time")


# ---------- PROBLEM CONFIG (opsiyonel cfg.json) ----------
# cfg.json alanlari (hepsi opsiyonel):
#   build_cmd    : ozel derleme komutu (framework). Ornek: "g++ -O2 -std=c++17 main.cpp -o code"
#   kalite_glob  : agent'in yazdigi dosyalar (kalite olcumu bunlarda). Ornek: ["qoi.h"]
#   correctness  : "generic" (testler/*.in vs *.out) | "custom" (oracle.py calistirilir)
#   mem_limit_mb : bellek limiti (MLE esigi)
def load_cfg():
    p = os.path.join(HERE, "cfg.json")
    if os.path.isfile(p):
        try:
            return json.load(open(p, encoding="utf-8"))
        except Exception:
            pass
    return {}

CFG = load_cfg()
if CFG.get("mem_limit_mb"):
    MEM_LIMIT_MB = int(CFG["mem_limit_mb"])


class Tee:
    def __init__(self, *streams): self.streams = streams
    def write(self, s):
        for st in self.streams: st.write(s)
    def flush(self):
        for st in self.streams: st.flush()


# ---------- kayit.csv ----------
def find_kayit():
    d = HERE
    for _ in range(6):
        p = os.path.join(d, "kayit.csv")
        if os.path.isfile(p): return p
        nd = os.path.dirname(d)
        if nd == d: break
        d = nd
    return None

def problem_id():
    m = re.search(r"problem-(\w+)", os.path.basename(HERE))
    return m.group(1) if m else ""

def read_kayit(pid):
    path = find_kayit()
    if not path: return {}
    out = {}
    try:
        with open(path, newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                rp = (row.get("problem") or "").strip()
                if rp == pid or rp.lstrip("0") == pid.lstrip("0"):
                    out[(row.get("arac") or "").strip()] = row
    except Exception: pass
    return out


def goreli(yol):
    """Raporlara repo kokune gore GORELI yol yaz (kullanici adi / yerel klasor yapisi sizmasin)."""
    return os.path.relpath(yol, os.path.abspath(os.path.join(HERE, "..", "..")))


def baslik(s):
    print("\n" + "=" * 64); print(s); print("=" * 64)


_JUNK_DIR = {"CMakeFiles", "build", "cmake-build-debug", "cmake-build-release", ".git", "_deps"}

def _junk_yol(p):
    """CMake/derleme cikti dosyasi mi? (CMakeCXXCompilerId.cpp gibi cop -> analize girmemeli)"""
    parts = p.replace("\\", "/").split("/")
    return any(x in _JUNK_DIR for x in parts)


def kaynaklar(sol_dir):
    s = []
    for ext in (".cpp", ".c", ".cc"):
        s += glob.glob(os.path.join(sol_dir, "**", "*" + ext), recursive=True)
    return [f for f in s if not _junk_yol(f)]


def kalite_kaynaklar(sol_dir):
    """Kalite olculecek dosyalar: cfg.kalite_glob varsa SADECE onlar (agent'in yazdigi),
    yoksa tum kaynak + header. Her durumda CMake/derleme copu haric."""
    globs = CFG.get("kalite_glob")
    if globs:
        out = []
        for g in globs:
            out += glob.glob(os.path.join(sol_dir, "**", g), recursive=True)
        return [f for f in out if not _junk_yol(f)]
    s = []
    for ext in (".cpp", ".c", ".cc", ".h", ".hpp"):
        s += glob.glob(os.path.join(sol_dir, "**", "*" + ext), recursive=True)
    return [f for f in s if not _junk_yol(f)]


# ---------- DERLEME ----------
def build(sol_dir, bdir):
    """HER ZAMAN KAYNAKTAN, TEMIZ derleme. bdir = olcum.temiz_kopya(sol_dir): cozumun git'te
    izlenen dosyalarinin taze kopyasi (eski 'code' binary'si, CMake cache, build/ YOK).
    Hazir binary asla kullanilmaz (017-antigravity'de bayat binary test edilmisti)."""
    code_path = os.path.join(bdir, "code")
    # FRAMEWORK: cfg.build_cmd varsa onu kullan
    bc = CFG.get("build_cmd")
    if bc:
        if CFG.get("kalite_glob"):
            has = any(glob.glob(os.path.join(bdir, "**", g), recursive=True) for g in CFG["kalite_glob"])
            if not has:
                return None, "agent dosyasi yok (arac henuz calistirilmadi mi?)"
        r = subprocess.run(bc, shell=True, cwd=bdir, capture_output=True, text=True)
        if os.path.isfile(code_path):
            return code_path, "temiz derleme: cfg build_cmd"
        try:
            with open(os.path.join(sol_dir, "derleme_hata.log"), "w") as f: f.write(r.stderr)
        except Exception: pass
        return None, "DERLEME HATASI (tam: derleme_hata.log): " + r.stderr.strip()[:150]
    # STANDALONE (varsayilan)
    sources = kaynaklar(bdir)
    if not sources:
        return None, "kaynak kod yok (arac henuz calistirilmadi mi?)"
    if os.path.isfile(os.path.join(bdir, "CMakeLists.txt")):
        subprocess.run(["cmake", "."], cwd=bdir, capture_output=True, text=True)
        subprocess.run(["make"], cwd=bdir, capture_output=True, text=True)
        if os.path.isfile(code_path):
            return code_path, "temiz derleme: cmake+make"
    r = subprocess.run(["g++", "-O2", "-std=c++17", "-o", code_path] + sources,
                       capture_output=True, text=True)
    if r.returncode == 0 and os.path.isfile(code_path):
        return code_path, "temiz derleme: g++ fallback"
    # tam derleme hatasini dosyaya kaydet
    try:
        with open(os.path.join(sol_dir, "derleme_hata.log"), "w") as f:
            f.write(r.stderr)
    except Exception: pass
    return None, "DERLEME HATASI (tam: derleme_hata.log): " + r.stderr.strip()[:150]


def derleyici_uyari(sol_dir):
    """-Wall -Wextra ile uyari sayisi. Framework'te sadece agent dosyasini iceren uyarilar."""
    kalite = kalite_kaynaklar(sol_dir)
    if not kalite: return None
    try:
        if CFG.get("build_cmd"):
            # framework: tam build'i -Wall -Wextra ile (ayri temiz kopyada), agent dosyasina ait uyarilari say
            cmd = re.sub(r"\bg\+\+\b", "g++ -Wall -Wextra", CFG["build_cmd"], count=1)
            # sol_dir burada zaten temiz kopya; build artigi karismasin diye ayrica kopyala
            wdir = os.path.join(os.path.dirname(sol_dir), os.path.basename(sol_dir) + "_uyari")
            shutil.copytree(sol_dir, wdir, ignore=shutil.ignore_patterns("code", "build", "CMakeFiles",
                                                                        "CMakeCache.txt"))
            try:
                r = subprocess.run(cmd, shell=True, cwd=wdir, capture_output=True, text=True, timeout=120)
            finally:
                shutil.rmtree(wdir, ignore_errors=True)
            adlar = [os.path.basename(f) for f in kalite]
            cnt = sum(1 for line in r.stderr.splitlines()
                      if "warning:" in line and any(n in line for n in adlar))
        else:
            r = subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-fsyntax-only"] + kalite,
                               capture_output=True, text=True, timeout=60)
            cnt = r.stderr.count("warning:")
        if r.stderr.strip():
            try:
                with open(os.path.join(sol_dir, "derleme_uyari.log"), "w") as f:
                    f.write(r.stderr)
            except Exception: pass
        return cnt
    except Exception:
        return None


# ---------- DOGRULUK + BELLEK + HIZ ----------
def run_one(exe, girdi):
    try:
        t0 = time.monotonic()
        if HAS_TIME:
            r = subprocess.run(["/usr/bin/time", "-v", exe], input=girdi,
                               capture_output=True, text=True, timeout=TIME_LIMIT)
            ms = (time.monotonic() - t0) * 1000
            rss = None
            m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", r.stderr)
            if m: rss = int(m.group(1))
            return r.stdout.strip(), rss, ("RE" if r.returncode != 0 else "OK"), ms
        else:
            r = subprocess.run([exe], input=girdi, capture_output=True, text=True, timeout=TIME_LIMIT)
            ms = (time.monotonic() - t0) * 1000
            return r.stdout.strip(), None, ("RE" if r.returncode != 0 else "OK"), ms
    except subprocess.TimeoutExpired:
        return "", None, "TLE", TIME_LIMIT * 1000


LIM = {}   # resmi limitli sonuc: gecen, toplam, tle, mle, cpu_ms, rss_kb (oracle LIMIT satirindan)

def dogruluk(exe):
    tests = sorted(glob.glob(os.path.join(TEST_DIR, "*.in")))
    passed = passed_lim = 0; detay = []; max_rss = 0; max_ms = 0
    L = olcum.Limit(problem_id())
    for tin in tests:
        ad = os.path.basename(tin)[:-3]
        girdi = open(tin).read()
        beklenen = open(tin[:-3] + ".out").read().strip()
        cikti, rss, verdict, ms = run_one(exe, girdi)
        if rss: max_rss = max(max_rss, rss)
        max_ms = max(max_ms, ms)
        if rss and rss > MEM_LIMIT_MB * 1024:
            detay.append(f"  Test {ad}: MLE ({rss//1024} MB > {MEM_LIMIT_MB} MB)")
        elif verdict == "TLE":
            detay.append(f"  Test {ad}: TLE (zaman asimi)")
        elif verdict == "RE":
            detay.append(f"  Test {ad}: RE (runtime error), gelen={cikti!r}")
        elif cikti == beklenen:
            passed += 1
            # resmi limit: CPU suresi + RSS ayrica olculur
            try:
                lim_ok = L.ok(olcum.run([exe], input=girdi, timeout=TIME_LIMIT))
            except subprocess.TimeoutExpired:
                lim_ok = L.ok(None)
            passed_lim += int(lim_ok)
            detay.append(f"  Test {ad}: PASS  (gelen={cikti!r}, {ms:.0f} ms" + (f", {rss//1024} MB)" if rss else ")")
                         + ("" if lim_ok else " [resmi limit asildi]"))
        else:
            detay.append(f"  Test {ad}: WA  (beklenen={beklenen!r}, gelen={cikti!r})")
    LIM.update(gecen=passed_lim, toplam=len(tests), tle=L.tle, mle=L.mle,
               cpu_ms=L.max_cpu_ms, rss_kb=L.max_rss_kb)
    return passed, len(tests), detay, max_rss, max_ms


CUSTOM_LEAK = None   # framework oracle'i leak_byte bildirirse buraya yazilir (yoksa None -> '-')

def custom_dogruluk(exe):
    """cfg.correctness=='custom' ise problem'in oracle.py'sini calistirir.
    oracle.py cikti formati: detay + son satir 'SONUC gecen=X toplam=N [ms=M] [rss_kb=R] [leak_byte=L]'
    leak_byte: valgrind 'definitely lost' (framework'te kucuk bir testte olculur)."""
    global CUSTOM_LEAK
    CUSTOM_LEAK = None
    oracle = os.path.join(HERE, "oracle.py")
    if not os.path.isfile(oracle):
        return 0, 0, ["  (oracle.py bulunamadi)"], 0, 0
    try:
        r = subprocess.run(["python3", oracle, exe], capture_output=True, text=True, timeout=600, cwd=HERE)
    except Exception as e:
        return 0, 0, [f"  (oracle hatasi: {e})"], 0, 0
    gecen = toplam = 0; max_rss = 0; max_ms = 0; detay = []
    for line in (r.stdout + r.stderr).splitlines():
        ml = re.match(r"LIMIT gecen=(\d+) toplam=(\d+) tl_ms=\d+ ml_mb=\d+ tle=(\d+) mle=(\d+) "
                      r"cpu_ms=([\d.]+) rss_kb=(\d+)", line)
        if ml:
            LIM.update(gecen=int(ml.group(1)), toplam=int(ml.group(2)), tle=int(ml.group(3)),
                       mle=int(ml.group(4)), cpu_ms=float(ml.group(5)), rss_kb=int(ml.group(6)))
            detay.append("  " + line)
            continue
        m = re.match(r"SONUC gecen=(\d+) toplam=(\d+)(?: ms=([\d.]+))?(?: rss_kb=(\d+))?(?: leak_byte=(-?\d+))?", line)
        if m:
            gecen = int(m.group(1)); toplam = int(m.group(2))
            if m.group(3): max_ms = float(m.group(3))
            if m.group(4): max_rss = int(m.group(4))
            if m.group(5) is not None and int(m.group(5)) >= 0: CUSTOM_LEAK = int(m.group(5))
        elif line.strip():
            detay.append("  " + line)
    return gecen, toplam, detay, max_rss, max_ms


def binary_kb(code_path):
    try: return os.path.getsize(code_path) // 1024
    except Exception: return "-"


# ---------- KOD / DOSYALAR / YORUM ----------
def kod_metrikleri(sol_dir):
    """satir/yorum SADECE agent dosyalarinda (kalite_kaynaklar). Scaffold sayilmaz."""
    kalite = set(os.path.abspath(f) for f in kalite_kaynaklar(sol_dir))
    framework = bool(CFG.get("kalite_glob"))
    dosyalar, toplam, yorum = [], 0, 0
    for root, _, files in os.walk(sol_dir):
        if any(x in root for x in (".git", "CMakeFiles", "build")):
            continue
        for f in files:
            if f in BUILD_JUNK: continue
            yol = os.path.join(root, f)
            rel = os.path.relpath(yol, sol_dir)
            is_kalite = os.path.abspath(yol) in kalite
            if f.endswith(SRC_EXT) and is_kalite:
                n = 0
                try:
                    for line in open(yol, errors="ignore"):
                        n += 1
                        s = line.strip()
                        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
                            yorum += 1
                except Exception: pass
                toplam += n
                dosyalar.append((rel + (" [agent]" if framework else ""), n))
            elif f not in (".gitkeep",):
                dosyalar.append((rel, None))
    yorum_orani = round(yorum / toplam * 100, 1) if toplam else 0
    return dosyalar, toplam, yorum_orani


def kendi_testi_var(sol_dir):
    """Arac kendi test dosyasi/klasoru olusturmus mu?"""
    for root, dirs, files in os.walk(sol_dir):
        if any(x in root for x in (".git", "CMakeFiles", "build")): continue
        if "test" in os.path.basename(root).lower() and os.path.basename(root) != os.path.basename(sol_dir):
            return True
        for f in files:
            fl = f.lower()
            if "test" in fl and f.endswith((".cpp", ".c", ".py", ".sh")) and "solution" not in fl:
                return True
    return False


# ---------- GIT ----------
def blame_survival(sol_dir, src):
    """Final koddaki satirlarin kaci EN ESKI commit'ten hayatta kalmis (%).
    HEAD uzerinden blame: calisma agacindaki CRLF/commit-edilmemis gurultuyu atlar,
    sadece commit'lenmis gecmise bakar."""
    out = subprocess.run(["git", "-C", sol_dir, "blame", "--line-porcelain", "HEAD", "--", src],
                         capture_output=True, text=True).stdout
    sha_time = {}; line_sha = []; cur = None
    for line in out.split("\n"):
        m = re.match(r"^([0-9a-f]{40}) \d+ \d+", line)
        if m: cur = m.group(1)
        elif line.startswith("committer-time ") and cur:
            sha_time[cur] = int(line.split()[1])
        elif line.startswith("\t"):
            line_sha.append(cur)
    if not line_sha or not sha_time: return None
    earliest = min(sha_time, key=sha_time.get)
    return round(sum(1 for s in line_sha if s == earliest) / len(line_sha) * 100, 1)


def git_metrikleri(sol_dir):
    if not os.path.isdir(os.path.join(sol_dir, ".git")): return None
    def g(a): return subprocess.run(["git", "-C", sol_dir] + a, capture_output=True, text=True).stdout.strip()
    commit = g(["rev-list", "--count", "HEAD"]) or "0"
    if not commit.isdigit() or int(commit) == 0:
        # git init var ama commit yok (orn. codex) -> olculecek gecmis yok
        return {"commit": "0", "mesajlar": "", "eklenen": None, "silinen": None, "survival": None}
    mesajlar = g(["log", "--pretty=format:  - %s"])
    stat = g(["log", "--pretty=tformat:", "--numstat"])
    # FRAMEWORK modu: churn/survival SADECE agent'in dosyalarinda (kalite_glob, orn qoi.h).
    # Scaffold (main.cpp/conv.h/utils.h) agent yazmadigi icin haric.
    kal_glob = CFG.get("kalite_glob")
    kal_isim = set(os.path.basename(f) for f in kalite_kaynaklar(sol_dir)) if kal_glob else None
    ekle = sil = 0
    for s in stat.splitlines():
        p = s.split("\t")
        if len(p) == 3 and p[0].isdigit() and p[1].isdigit():
            bn = os.path.basename(p[2])
            if kal_isim is not None:
                if bn not in kal_isim: continue      # framework: sadece agent dosyalari
            elif bn in SEED_FILES: continue           # standalone: seed dosyalari haric
            ekle += int(p[0]); sil += int(p[1])
    # NOT: agent calisma suresi (span) ARTIK OLCULMUYOR - onay bekleme + internet
    # gecikmesi commit zaman damgalarini kirletiyor, guvenilir degil.
    # satir survival: framework'te agent dosyasinda (qoi.h), yoksa ilk kaynak dosyada
    survival = None
    surv_kaynak = kalite_kaynaklar(sol_dir) if kal_glob else kaynaklar(sol_dir)
    for f in surv_kaynak:
        rel = os.path.relpath(f, sol_dir)
        s = blame_survival(sol_dir, rel)
        if s is not None: survival = s; break
    return {"commit": commit, "mesajlar": mesajlar, "eklenen": ekle, "silinen": sil,
            "survival": survival}


# ---------- LIZARD / SEMGREP / VALGRIND / CLANG-TIDY ----------
def lizard_ccn(sol_dir):
    """(avg_ccn, max_ccn) doner."""
    if not shutil.which("lizard"): return None, None
    kalite = kalite_kaynaklar(sol_dir)
    if not kalite: return None, None
    try:
        r = subprocess.run(["lizard"] + kalite, capture_output=True, text=True, timeout=30)
    except Exception:
        return None, None
    lines = r.stdout.splitlines()
    avg = None; ccns = []
    in_fn = False
    for i, line in enumerate(lines):
        if re.match(r"\s*NLOC\s+CCN\s+token", line):
            in_fn = True; continue
        if in_fn:
            if line.startswith("---"): continue
            m = re.match(r"\s*\d+\s+(\d+)\s+\d+", line)
            if m: ccns.append(int(m.group(1)))
            elif "analyzed" in line: in_fn = False
        if "Total nloc" in line and "AvgCCN" in line:
            for j in range(i + 1, len(lines)):
                if lines[j].startswith("---"): continue
                nums = re.findall(r"[\d.]+", lines[j])
                if len(nums) >= 3: avg = float(nums[2]); break
            break
    return avg, (max(ccns) if ccns else None)


def cppcheck_say(sol_dir):
    """Guvenlik/bug bulgu sayisi (cppcheck - offline, C/C++ icin semgrep'ten cok daha saglam).
    error+warning severity sayilir: buffer overflow, uninitialized, leak, null deref, vb.
    (Not: semgrep --config=auto C/C++'ta offline calismiyordu, sahte 0 veriyordu - degistirildi.)"""
    if not shutil.which("cppcheck"): return None
    kalite = kalite_kaynaklar(sol_dir)
    if not kalite: return None
    try:
        r = subprocess.run(["cppcheck", "--enable=warning", "--inconclusive", "--quiet",
                            "--language=c++", "--template={severity}"] + kalite,
                           capture_output=True, text=True, timeout=120)
    except Exception:
        return None
    # cppcheck bulgulari stderr'e severity satiri olarak yazar
    return sum(1 for ln in r.stderr.splitlines() if ln.strip() in ("error", "warning"))


def valgrind_leak(exe, girdi):
    """definitely lost (bytes). None=valgrind yok."""
    if not shutil.which("valgrind"): return None
    try:
        r = subprocess.run(["valgrind", "--leak-check=full", "--error-exitcode=0", exe],
                           input=girdi, capture_output=True, text=True, timeout=60)
        m = re.search(r"definitely lost:\s*([\d,]+)\s*bytes", r.stderr)
        return int(m.group(1).replace(",", "")) if m else 0
    except Exception:
        return None


def clang_tidy_say(sol_dir):
    """clang-tidy warning sayisi. ONEMLI: .h/.hpp dosyalari default C sanilir ->
    '-x c++' ile C++ zorla + include yollari ver, yoksa <cstdint> bulunamaz ve
    DERLEME HATASI -> sahte 0 verir (semgrep gibi). Derleme hatasi olursa None (analiz gecersiz)."""
    if not shutil.which("clang-tidy"): return None
    kalite = kalite_kaynaklar(sol_dir)
    if not kalite: return None
    incdirs = {sol_dir} | {os.path.dirname(f) for f in kalite}
    inc = []
    for d in incdirs: inc += ["-I", d]
    try:
        r = subprocess.run(["clang-tidy"] + kalite + ["--", "-x", "c++", "-std=c++17"] + inc,
                           capture_output=True, text=True, timeout=120)
    except Exception:
        return None
    out = r.stdout + r.stderr
    if "clang-diagnostic-error" in out or "Found compiler error" in out:
        return None   # analiz edilemedi -> sahte 0 verme
    return out.count("warning:")


def cr_proxy(avg_ccn, sg, derlendi):
    """Statik CR skoru (0-100)."""
    if not derlendi: return 0.0, "derlenemedi"
    cr = 100.0; notlar = []
    if avg_ccn is not None:
        if avg_ccn > 10:
            ceza = min(40, (avg_ccn - 10) * 4); cr -= ceza; notlar.append(f"CCN={avg_ccn:.1f}(-{ceza:.0f})")
        else: notlar.append(f"CCN={avg_ccn:.1f}")
    if sg:
        ceza = min(40, sg * 10); cr -= ceza; notlar.append(f"{sg} guvenlik(-{ceza:.0f})")
    if avg_ccn is None and sg is None:
        return None, "lizard/cppcheck yok"
    return max(0, cr), ", ".join(notlar) if notlar else "temiz"


# ---------- ANA ----------
def main():
    rapor_yol = os.path.join(HERE, "rapor.txt")
    orig = sys.stdout
    rf = open(rapor_yol, "w", encoding="utf-8")
    sys.stdout = Tee(orig, rf)
    try:
        _main()
        print(f"\nDetayli rapor: {goreli(rapor_yol)}")
    finally:
        sys.stdout = orig
        rf.close()


def _main():
    if not os.path.isdir(SONUC_DIR):
        print("HATA: sonuclar/ yok."); return
    araclar = sorted(d for d in os.listdir(SONUC_DIR)
                     if os.path.isdir(os.path.join(SONUC_DIR, d)) and d not in ATLA)
    if not araclar:
        print("sonuclar/ bos."); return
    pid = problem_id()
    kayit = read_kayit(pid)
    print(f"(problem {pid} · kayit.csv'den {len(kayit)} arac eslesti)")
    if not HAS_TIME: print("(not: /usr/bin/time yok -> bellek/hiz kisitli)")

    # ilk test girdisi (valgrind icin)
    ilk_test = sorted(glob.glob(os.path.join(TEST_DIR, "*.in")))
    ilk_girdi = open(ilk_test[0]).read() if ilk_test else ""

    ozet = []
    for arac in araclar:
        sol = os.path.join(SONUC_DIR, arac)
        baslik(f"ARAC: {arac}")
        # repodaki kaynaktan taze kopya (cfg.sabit_surum varsa o commit'ten). Derleme, testler ve
        # TUM kod metrikleri bu kopya uzerinde -> olculen kaynak == test edilen kaynak.
        surum = (CFG.get("sabit_surum") or {}).get(arac)
        bdir = olcum.temiz_kopya(sol, surum)
        if surum:
            print(f"Kaynak surumu sabit: commit {surum} (cfg.sabit_surum)")
        LIM.clear()
        exe, durum = build(sol, bdir)
        print(f"Derleme: {durum}")

        if exe:
            dg = custom_dogruluk if CFG.get("correctness") == "custom" else dogruluk
            gecen, toplam, detay, max_rss, max_ms = dg(exe)
            exec_score = gecen / toplam * 100 if toplam else 0
            print(f"\nDogruluk: {gecen}/{toplam}  (exec={exec_score:.0f}/100)")
            for d in detay: print(d)
            print(f"Max bellek: {max_rss//1024 if max_rss else '?'} MB · Max hiz: {max_ms:.0f} ms")
        else:
            if CFG.get("correctness") == "custom":
                # build basarisiz -> oracle yine de test sayisini bildirir (0/N, 0/0 degil).
                # exe yolu temiz kopyada -> var olmayan binary (eski 'code' ASLA calistirilmaz)
                gecen, toplam, detay, _, _ = custom_dogruluk(os.path.join(bdir, "code"))
            else:
                gecen, toplam = 0, len(ilk_test)
            exec_score, max_rss, max_ms = 0, 0, 0
            print(f"\nDogruluk: {gecen}/{toplam} (derlenemedi)")
        # resmi zaman/bellek limitleriyle (ProjDevBench) dogruluk
        if LIM.get("toplam"):
            exec_lim = LIM["gecen"] / LIM["toplam"] * 100
        else:
            exec_lim = 0.0 if gecen == 0 else None
        tl_ms, ml_mb = olcum.LIMITS.get(pid, (None, None))
        if exec_lim is not None:
            print(f"Resmi limitlerle (TL {tl_ms} ms CPU, ML {ml_mb} MiB): exec_lim={exec_lim:.1f} "
                  f"· TLE={LIM.get('tle', 0)} MLE={LIM.get('mle', 0)} "
                  f"· max CPU {LIM.get('cpu_ms', 0):.0f} ms · max RSS {LIM.get('rss_kb', 0)//1024} MB")
        if not max_rss and LIM.get("rss_kb"):
            max_rss = LIM["rss_kb"]

        # statik + ek metrikler
        uyari = derleyici_uyari(bdir)
        bkb = binary_kb(exe) if exe else "-"
        avg_ccn, max_ccn = lizard_ccn(bdir)
        sg = cppcheck_say(bdir)
        tidy = clang_tidy_say(bdir)
        if CFG.get("correctness") == "custom":
            leak = CUSTOM_LEAK                       # framework: oracle valgrind ile olctu (kucuk test)
        else:
            leak = valgrind_leak(exe, ilk_girdi) if exe else None
        dosyalar, satir, yorum_orani = kod_metrikleri(bdir)
        kendi_test = kendi_testi_var(bdir)
        cr, crnot = cr_proxy(avg_ccn, sg, exe is not None)

        print(f"\nDerleyici uyari (-Wall -Wextra): {uyari if uyari is not None else '-'}")
        print(f"Binary: {bkb} KB · Complexity avg/max: {avg_ccn}/{max_ccn} · Yorum: %{yorum_orani}")
        print(f"Guvenlik (cppcheck): {sg if sg is not None else '-'} · clang-tidy: {tidy if tidy is not None else '-'}")
        print(f"Bellek sizintisi (valgrind): {leak if leak is not None else '-'} byte · Kendi testi: {'var' if kendi_test else 'yok'}")
        if cr is not None:
            combined = 0.8 * exec_score + 0.2 * cr
            print(f"Code Review (proxy): {cr:.0f}/100 ({crnot}) · BIRLESIK: {combined:.1f}/100")
        else:
            combined = exec_score
            print(f"CR hesaplanamadi ({crnot}) · BIRLESIK = exec = {exec_score:.0f}")

        print(f"\nKod: {satir} satir, {len(dosyalar)} dosya")
        for rel, n in dosyalar:
            print(f"  {rel}" + (f"  ({n} satir)" if n is not None else ""))
        g = git_metrikleri(sol)
        if g and g["eklenen"] is not None:
            rewrite = round(g["eklenen"] / satir, 2) if satir else "-"
            print(f"\nGit: {g['commit']} commit, +{g['eklenen']}/-{g['silinen']} (churn)")
            print(f"  Satir survival: {g['survival'] if g['survival'] is not None else '-'}% (ilk commit'ten kalan) · Rewrite orani: {rewrite}x")
            if g["mesajlar"]: print("  Commit mesajlari:\n" + g["mesajlar"])
        elif g:
            rewrite = "-"
            print(f"\nGit: {g['commit']} commit (commit yok -> churn/survival olculemedi)")
        else:
            rewrite = "-"
            print("\nGit: repo yok")

        ozet.append({
            "problem": pid, "arac": arac, "model": kayit.get(arac, {}).get("model", "-"),
            "dogruluk": f"{gecen}/{toplam}", "exec_score": round(exec_score, 1),
            "cr_score": round(cr, 1) if cr is not None else "-",
            "guvenlik": sg if sg is not None else "-",
            "uyari": uyari if uyari is not None else "-",
            "clang_tidy": tidy if tidy is not None else "-",
            "leak_byte": leak if leak is not None else "-",
            "hiz_ms": round(max_ms) if exe else "-",
            "binary_kb": bkb, "max_ccn": max_ccn if max_ccn is not None else "-",
            "yorum_orani": yorum_orani, "kendi_testi": "var" if kendi_test else "yok",
            "birlesik": round(combined, 1), "max_mb": max_rss // 1024 if max_rss else "-",
            "satir": satir, "commit": g["commit"] if g else "-",
            "churn": f"+{g['eklenen']}/-{g['silinen']}" if g and g["eklenen"] is not None else "-",
            "survival": g["survival"] if g and g["survival"] is not None else "-",
            "rewrite": rewrite,
            "tur": kayit.get(arac, {}).get("tur_sayisi", "-"),
            "mudahale": kayit.get(arac, {}).get("mudahale", "-"),
            "not": (kayit.get(arac, {}).get("not", "-") or "-")[:40],
            "exec_lim": round(exec_lim, 1) if exec_lim is not None else "-",
            "tle": LIM.get("tle", "-") if exe else "-",
            "mle": LIM.get("mle", "-") if exe else "-",
            "cpu_ms": round(LIM["cpu_ms"]) if exe and "cpu_ms" in LIM else "-",
        })
        shutil.rmtree(bdir, ignore_errors=True)

    # OZET (ekran kompakt)
    baslik("OZET TABLO (kompakt · tam veri sonuclar.csv'de)")
    kol_ekran = ["arac", "model", "dogruluk", "birlesik", "uyari", "hiz_ms", "guvenlik", "leak_byte", "churn"]
    gen = {c: max(len(c), *(len(str(o.get(c, "-"))) for o in ozet)) for c in kol_ekran}
    print("  ".join(c.ljust(gen[c]) for c in kol_ekran))
    print("  ".join("-" * gen[c] for c in kol_ekran))
    for o in ozet:
        print("  ".join(str(o.get(c, "-")).ljust(gen[c]) for c in kol_ekran))

    kol_csv = ["problem", "arac", "model", "dogruluk", "exec_score", "cr_score", "guvenlik",
               "uyari", "clang_tidy", "leak_byte", "hiz_ms", "binary_kb", "max_ccn", "yorum_orani",
               "kendi_testi", "birlesik", "max_mb", "satir", "commit", "churn", "survival",
               "rewrite", "tur", "mudahale", "not", "exec_lim", "tle", "mle", "cpu_ms"]
    csv_yol = os.path.join(HERE, "sonuclar.csv")
    with open(csv_yol, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=kol_csv); w.writeheader(); w.writerows(ozet)
    print(f"\nTAM veri -> {goreli(csv_yol)}")

    # master birlesik CSV'yi otomatik tazele (kok/birlestir.py)
    kok = os.path.abspath(os.path.join(HERE, "..", ".."))
    birlestir = os.path.join(kok, "birlestir.py")
    if os.path.isfile(birlestir):
        try:
            subprocess.run(["python3", birlestir], cwd=kok, timeout=60)
        except Exception as e:
            print(f"(master birlestirme atlandi: {e})")


if __name__ == "__main__":
    main()
