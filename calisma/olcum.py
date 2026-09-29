#!/usr/bin/env python3
"""
Ortak olcum modulu - oracle.py'ler ve analiz.py tarafindan kullanilir.

1) run(): bir programi /usr/bin/time altinda calistirir; CPU suresi (user+sys, ms) ve
   tepe bellek (max RSS, KB) OLCULUR. Zaman asiminda tum surec grubu oldurulur.
2) Limit: ProjDevBench'in RESMI test-basi zaman/bellek limitlerini uygular (TLE/MLE).
   Ajanlari yeniden kosturmadan, uretilmis kod uzerinde sonradan uygulanir.
3) temiz_kopya(): cozum klasorunun SADECE git'te izlenen dosyalarini gecici bir dizine
   kopyalar -> derleme her zaman repodaki kaynaktan yapilir (eski binary / CMake cache yok).

Limit secimi: README'de aralik verilen problemlerde (orn. "5000 ms min, 40000 ms max")
EN GEVSEK (max) deger kullanilir. Boylece raporlanan her TLE/MLE, resmi judge'da da
(donanim farki disinda) ihlal olurdu -> ihlal sayilari ALT SINIRDIR (muhafazakar).
Alt gorevi belli olan problemde (006 Basic server) o alt gorevin limiti kullanilir.
Kaynak: projdevbench/problem/<id>/README.md "Per-Testcase Resource Limits".
"""
import os, re, signal, shutil, subprocess, tempfile

# problem -> (zaman limiti ms [CPU], bellek limiti MiB)
LIMITS = {
    "001": (1000, 256),
    "002": (10000, 190),    # alt gorevler 1000-10000 ms / 47-190 MiB
    "004": (10000, 64),
    "005": (10000, 512),    # 2000-10000 ms
    "006": (1000, 244),     # 2876 Basic (test edilen server hedefi)
    "009": (100000, 768),   # 250-100000 ms / 512-768 MiB
    "013": (30000, 893),    # 2671+2672 birlikte: 100-30000 ms / 512-893 MiB
    "014": (16000, 512),    # 500-16000 ms
    "017": (40000, 47),     # 5000-40000 ms / 42-47 MiB
    "018": (1500, 244),
    "019": (1000, 244),
    "020": (10000, 244),
}


class Sonuc:
    def __init__(self, stdout, stderr, returncode, cpu_ms, rss_kb):
        self.stdout, self.stderr, self.returncode = stdout, stderr, returncode
        self.cpu_ms, self.rss_kb = cpu_ms, rss_kb


def run(cmd, input=None, stdin=None, cwd=None, timeout=None, text=True):
    """cmd'yi /usr/bin/time ile calistir. Zaman asiminda subprocess.TimeoutExpired firlatir
    (surec grubu olduruldukten sonra). Donus: Sonuc(stdout, stderr, returncode, cpu_ms, rss_kb)."""
    fd, tf = tempfile.mkstemp(prefix="olcum_")
    os.close(fd)
    full = ["/usr/bin/time", "-f", "%U %S %M", "-o", tf] + list(cmd)
    p = subprocess.Popen(full, stdin=subprocess.PIPE if input is not None else stdin,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=cwd,
                         text=text, start_new_session=True)
    try:
        out, err = p.communicate(input=input, timeout=timeout)
    except subprocess.TimeoutExpired:
        try: os.killpg(p.pid, signal.SIGKILL)
        except Exception: pass
        p.communicate()
        os.unlink(tf)
        raise
    cpu_ms = rss_kb = None
    try:
        # program sinyalle olurse ilk satir "Command terminated by signal N" olur -> son satiri al
        son = open(tf).read().strip().splitlines()[-1].split()
        cpu_ms = (float(son[0]) + float(son[1])) * 1000
        rss_kb = int(son[2])
    except Exception:
        pass
    os.unlink(tf)
    return Sonuc(out, err, p.returncode, cpu_ms, rss_kb)


class Limit:
    """Resmi limitleri say. ok(r): r (Sonuc) limitler icindeyse True; None = zaman asimi."""
    def __init__(self, pid):
        self.tl_ms, self.ml_mb = LIMITS[pid]
        self.tle = self.mle = 0
        self.max_cpu_ms = 0.0
        self.max_rss_kb = 0

    def ok(self, r):
        if r is None:
            self.tle += 1
            return False
        if r.cpu_ms is not None:
            self.max_cpu_ms = max(self.max_cpu_ms, r.cpu_ms)
        if r.rss_kb is not None:
            self.max_rss_kb = max(self.max_rss_kb, r.rss_kb)
        if r.cpu_ms is not None and r.cpu_ms > self.tl_ms:
            self.tle += 1
            return False
        if r.rss_kb is not None and r.rss_kb > self.ml_mb * 1024:
            self.mle += 1
            return False
        return True

    def satir(self, gecen_lim, toplam):
        """analiz.py'nin parse ettigi satir."""
        return (f"LIMIT gecen={gecen_lim} toplam={toplam} tl_ms={self.tl_ms} ml_mb={self.ml_mb} "
                f"tle={self.tle} mle={self.mle} cpu_ms={self.max_cpu_ms:.0f} rss_kb={self.max_rss_kb}")


_COP = {"code", "code.exe", "a.out", "CMakeCache.txt", "CMakeFiles", "build", "cmake_install.cmake",
        "cmake-build-debug", "cmake-build-release", "_deps"}


def _git(sol_dir, *args, **kw):
    return subprocess.run(["git", "-c", "safe.directory=*", "-C", sol_dir] + list(args),
                          capture_output=True, **kw)


def temiz_kopya(sol_dir, commit=None):
    """sol_dir'in git'te IZLENEN dosyalarini yeni bir gecici dizine kopyala ve dizini dondur.
    commit verilirse dosyalar calisma agacindan degil O COMMIT'ten alinir (git archive):
    ajanin teslim ettigi surum, sonradan yapilan duzenlemelerden bagimsiz degerlendirilir.
    git kullanilamazsa derleme copu haric her seyi kopyalar. Derleme artiklari (code, CMake
    cache, build/) her durumda silinir -> derleme daima kaynaktan."""
    hedef = tempfile.mkdtemp(prefix="temiz_")
    if commit:
        prefix = _git(sol_dir, "rev-parse", "--show-prefix", text=True).stdout.strip().rstrip("/")
        kok = _git(sol_dir, "rev-parse", "--show-toplevel", text=True).stdout.strip()
        # kok dizinden cagir: alt dizinden cagrilinca git archive ciktiyi o dizine gore filtreler
        arc = _git(kok, "archive", "--format=tar", f"{commit}:{prefix}")
        if arc.returncode != 0:
            raise RuntimeError(f"git archive {commit}:{prefix} basarisiz: {arc.stderr[:200]}")
        subprocess.run(["tar", "-x", "-C", hedef], input=arc.stdout, check=True)
        dosyalar = None
    else:
        r = _git(sol_dir, "ls-files", "-z", "--", ".", text=True)
        dosyalar = [f for f in r.stdout.split("\0") if f] if r.returncode == 0 else []
    if dosyalar is None:
        pass
    elif dosyalar:
        for rel in dosyalar:
            src = os.path.join(sol_dir, rel)
            if not os.path.isfile(src):
                continue
            dst = os.path.join(hedef, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
    else:
        shutil.rmtree(hedef)
        shutil.copytree(sol_dir, hedef, ignore=shutil.ignore_patterns(".git", *_COP))
    for kok, dirs, files in os.walk(hedef, topdown=True):
        for d in list(dirs):
            if d in _COP:
                shutil.rmtree(os.path.join(kok, d), ignore_errors=True)
                dirs.remove(d)
        for f in files:
            if f in _COP or f == "Makefile" and os.path.isfile(os.path.join(kok, "CMakeLists.txt")):
                os.remove(os.path.join(kok, f))
    return hedef
