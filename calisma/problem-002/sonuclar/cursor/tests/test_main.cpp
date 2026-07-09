// Local test harness for the int2048 big-integer class.
//
// It combines:
//   1. Fixed known-answer checks (including floor-division corner cases).
//   2. Randomized cross-checks against `long long` for operands that fit.
//
// A separate script (tests/bigcheck.py) cross-checks very large operands
// against Python's arbitrary-precision integers via the `--stream` mode.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "int2048.h"

using sjtu::int2048;

static int g_failures = 0;
static long long g_checks = 0;

static std::string toStr(const int2048 &x) {
  std::ostringstream oss;
  oss << x;
  return oss.str();
}

static void expectEq(const std::string &got, const std::string &want,
                     const std::string &label) {
  ++g_checks;
  if (got != want) {
    ++g_failures;
    std::cout << "FAIL [" << label << "]  got=" << got << "  want=" << want
              << "\n";
  }
}

static std::string llToStr(long long v) {
  std::ostringstream oss;
  oss << v;
  return oss.str();
}

// Floor division / modulo on long long (Python semantics) for reference.
static long long floorDiv(long long a, long long b) {
  long long q = a / b;
  long long r = a % b;
  if (r != 0 && ((r < 0) != (b < 0))) --q;
  return q;
}
static long long floorMod(long long a, long long b) {
  return a - floorDiv(a, b) * b;
}

static void fixedTests() {
  expectEq(toStr(int2048(0)), "0", "ctor0");
  expectEq(toStr(int2048(-0)), "0", "ctorNeg0");
  expectEq(toStr(int2048(123456789LL)), "123456789", "ctorLL");
  expectEq(toStr(int2048(-987654321LL)), "-987654321", "ctorNegLL");
  expectEq(toStr(int2048((long long)-9223372036854775807LL - 1)),
           "-9223372036854775808", "ctorLLMIN");
  expectEq(toStr(int2048(std::string("  -00012300"))), "-12300",
           "readLeadingZeros");
  expectEq(toStr(int2048(std::string("+0"))), "0", "readPlusZero");

  // Addition / subtraction across signs.
  expectEq(toStr(int2048(std::string("999999999999999999")) +
                 int2048(std::string("1"))),
           "1000000000000000000", "carryChain");
  expectEq(toStr(int2048(5) - int2048(8)), "-3", "subToNeg");
  expectEq(toStr(int2048(-5) + int2048(5)), "0", "cancelToZero");
  expectEq(toStr(int2048(-5) - int2048(-8)), "3", "negMinusNeg");

  // Multiplication.
  expectEq(toStr(int2048(0) * int2048(std::string("123456789"))), "0",
           "mulZero");
  expectEq(toStr(int2048(-12) * int2048(12)), "-144", "mulSign");

  // Floor division / modulo, matching the README examples.
  expectEq(toStr(int2048(10) / int2048(3)), "3", "div++");
  expectEq(toStr(int2048(-10) / int2048(3)), "-4", "div-+");
  expectEq(toStr(int2048(10) / int2048(-3)), "-4", "div+-");
  expectEq(toStr(int2048(-10) / int2048(-3)), "3", "div--");
  expectEq(toStr(int2048(10) % int2048(3)), "1", "mod++");
  expectEq(toStr(int2048(-10) % int2048(3)), "2", "mod-+");
  expectEq(toStr(int2048(10) % int2048(-3)), "-2", "mod+-");
  expectEq(toStr(int2048(-10) % int2048(-3)), "-1", "mod--");

  // Small dividend, larger divisor (quotient 0 corner cases).
  expectEq(toStr(int2048(3) / int2048(-10)), "-1", "divSmall+-");
  expectEq(toStr(int2048(3) % int2048(-10)), "-7", "modSmall+-");
  expectEq(toStr(int2048(-3) / int2048(10)), "-1", "divSmall-+");
  expectEq(toStr(int2048(-3) % int2048(10)), "7", "modSmall-+");

  // Exact division.
  expectEq(toStr(int2048(100) / int2048(-10)), "-10", "divExactSign");
  expectEq(toStr(int2048(100) % int2048(-10)), "0", "modExactZero");

  // Comparisons.
  ++g_checks;
  if (!(int2048(-5) < int2048(3))) {
    ++g_failures;
    std::cout << "FAIL [cmpLt]\n";
  }
  ++g_checks;
  if (!(int2048(std::string("100000000")) > int2048(std::string("99999999")))) {
    ++g_failures;
    std::cout << "FAIL [cmpGtLimb]\n";
  }
}

static void randomSmallTests() {
  std::mt19937_64 rng(12345);
  // Range chosen so that sums stay within long long.
  std::uniform_int_distribution<long long> wide(-4000000000000000000LL,
                                                4000000000000000000LL);
  // Range chosen so that products stay within long long.
  std::uniform_int_distribution<long long> narrow(-2000000000LL, 2000000000LL);

  for (int iter = 0; iter < 300000; ++iter) {
    long long a = wide(rng);
    long long b = wide(rng);
    int2048 A(a), B(b);

    expectEq(toStr(A + B), llToStr(a + b), "randAdd");
    expectEq(toStr(A - B), llToStr(a - b), "randSub");
    if (b != 0) {
      expectEq(toStr(A / B), llToStr(floorDiv(a, b)), "randDiv");
      expectEq(toStr(A % B), llToStr(floorMod(a, b)), "randMod");
    }

    long long c = narrow(rng);
    long long d = narrow(rng);
    expectEq(toStr(int2048(c) * int2048(d)), llToStr(c * d), "randMul");
  }
}

// Read "<a> <op> <b>" and print the result, for external cross-checking.
static void streamMode() {
  int2048 a, b;
  std::string op;
  std::cin >> a >> op >> b;
  if (op == "+")
    std::cout << (a + b);
  else if (op == "-")
    std::cout << (a - b);
  else if (op == "*")
    std::cout << (a * b);
  else if (op == "/")
    std::cout << (a / b);
  else if (op == "%")
    std::cout << (a % b);
  std::cout << "\n";
  return;
}

static std::string randomDigits(int n, std::mt19937_64 &rng) {
  std::string s;
  s.reserve(n);
  std::uniform_int_distribution<int> first(1, 9), rest(0, 9);
  s.push_back(static_cast<char>('0' + first(rng)));
  for (int i = 1; i < n; ++i) s.push_back(static_cast<char>('0' + rest(rng)));
  return s;
}

static void perfMode() {
  using clock = std::chrono::steady_clock;
  std::mt19937_64 rng(99);

  auto timeIt = [](const char *label, auto fn) {
    auto t0 = clock::now();
    fn();
    auto t1 = clock::now();
    double ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << label << ": " << ms << " ms\n";
  };

  // 2017: multiplication, result up to 10^200000 (each operand ~100000 digits)
  {
    int2048 a(randomDigits(100000, rng)), b(randomDigits(100000, rng));
    int2048 c;
    timeIt("mul 100000x100000 digits", [&] { c = a * b; });
    std::cout << "  (result digits ~ " << toStr(c).size() << ")\n";
  }

  // 2019: multiplication, result up to 10^500000
  {
    int2048 a(randomDigits(250000, rng)), b(randomDigits(250000, rng));
    int2048 c;
    timeIt("mul 250000x250000 digits", [&] { c = a * b; });
    std::cout << "  (result digits ~ " << toStr(c).size() << ")\n";
  }

  // 2018: division, dividend 24000 digits / divisor 12000 digits
  {
    int2048 a(randomDigits(24000, rng)), b(randomDigits(12000, rng));
    int2048 q, r;
    timeIt("div 24000 / 12000 digits", [&] {
      q = a / b;
      r = a % b;
    });
  }

  // balanced large division (stress-style)
  {
    int2048 a(randomDigits(120000, rng)), b(randomDigits(60000, rng));
    int2048 q;
    timeIt("div 120000 / 60000 digits", [&] { q = a / b; });
  }
}

int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "--stream") {
    streamMode();
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--perf") {
    perfMode();
    return 0;
  }

  fixedTests();
  randomSmallTests();

  std::cout << "Checks: " << g_checks << ", Failures: " << g_failures << "\n";
  return g_failures == 0 ? 0 : 1;
}
