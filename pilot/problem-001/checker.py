#!/usr/bin/env python3
"""
Problem 001 (A+B) icin otomatik checker.

Kullanim:
    python3 checker.py <cozum_klasoru>

Yapar:
  1. Cozum klasorunde calistirilabilir 'code' arar; yoksa cmake/make ya da g++ ile derler.
  2. testler/ klasorundeki her *.in dosyasini koda verir, ciktiyi *.out ile karsilastirir.
  3. Kac test gecti raporlar (pass@1 orani).

Linux/WSL'de calistir (g++ gerektirir).
"""
import os
import sys
import subprocess
import glob

HERE = os.path.dirname(os.path.abspath(__file__))
TEST_DIR = os.path.join(HERE, "testler")
TIME_LIMIT = 5  # saniye (problem 1s ama derleme/IO payi)


def build(sol_dir):
    """Cozum klasorunde 'code' calistirilabilirini uret. Yolu dondur ya da None."""
    code_path = os.path.join(sol_dir, "code")

    # 1) Hazir 'code' var mi?
    if os.path.isfile(code_path) and os.access(code_path, os.X_OK):
        return code_path

    # 2) CMakeLists.txt varsa cmake + make
    if os.path.isfile(os.path.join(sol_dir, "CMakeLists.txt")):
        subprocess.run(["cmake", "."], cwd=sol_dir,
                       capture_output=True, text=True)
        subprocess.run(["make"], cwd=sol_dir,
                       capture_output=True, text=True)
        if os.path.isfile(code_path):
            return code_path

    # 3) Fallback: herhangi bir .cpp/.c dosyasini g++ ile derle
    sources = glob.glob(os.path.join(sol_dir, "**", "*.cpp"), recursive=True)
    sources += glob.glob(os.path.join(sol_dir, "**", "*.c"), recursive=True)
    if sources:
        r = subprocess.run(["g++", "-O2", "-std=c++17", "-o", code_path] + sources,
                           capture_output=True, text=True)
        if r.returncode == 0 and os.path.isfile(code_path):
            return code_path
        print("  [derleme hatasi]", r.stderr.strip()[:300])

    return None


def run_tests(exe):
    tests = sorted(glob.glob(os.path.join(TEST_DIR, "*.in")))
    passed = 0
    total = len(tests)
    for tin in tests:
        name = os.path.basename(tin)[:-3]
        with open(tin) as f:
            girdi = f.read()
        with open(tin[:-3] + ".out") as f:
            beklenen = f.read().strip()
        try:
            r = subprocess.run([exe], input=girdi, capture_output=True,
                               text=True, timeout=TIME_LIMIT)
            cikti = r.stdout.strip()
        except subprocess.TimeoutExpired:
            print(f"  Test {name}: TLE (zaman asimi)")
            continue
        if cikti == beklenen:
            passed += 1
            print(f"  Test {name}: PASS")
        else:
            print(f"  Test {name}: FAIL  (beklenen={beklenen!r}, gelen={cikti!r})")
    return passed, total


def main():
    if len(sys.argv) != 2:
        print("Kullanim: python3 checker.py <cozum_klasoru>")
        sys.exit(1)
    sol_dir = sys.argv[1]
    print(f"Cozum: {sol_dir}")

    exe = build(sol_dir)
    if not exe:
        print("SONUC: derlenemedi -> dogruluk 0/%d" % len(glob.glob(os.path.join(TEST_DIR, "*.in"))))
        sys.exit(0)

    passed, total = run_tests(exe)
    oran = passed / total if total else 0
    print(f"\nSONUC: {passed}/{total} test gecti  (dogruluk = {oran:.2%})")


if __name__ == "__main__":
    main()
