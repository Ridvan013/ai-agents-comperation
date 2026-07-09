#pragma once
#ifndef SJTU_BIGINTEGER
#define SJTU_BIGINTEGER

// Integer 1:
// Implement a signed big integer class that only needs to support simple addition and subtraction

// Integer 2:
// Implement a signed big integer class that supports addition, subtraction, multiplication, and division, and overload related operators

// Do not use any header files other than the following
#include <complex>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

// Do not use "using namespace std;"

namespace sjtu {
class int2048 {
private:
  // The magnitude is stored in little-endian order using base 10^kWidth limbs.
  // Every limb lies in the range [0, kBase). The vector is kept canonical:
  // it has no trailing (most-significant) zero limbs, and the value zero is
  // represented by an empty vector together with sign_ == 1.
  static constexpr int kWidth = 3;   // decimal digits packed per limb
  static constexpr int kBase = 1000; // 10^kWidth

  std::vector<int> digits_; // limbs, least-significant first
  int sign_ = 1;            // +1 or -1; zero always uses +1

  // ---- canonicalisation ----------------------------------------------------
  void trim();               // remove leading zero limbs; normalise sign of zero
  bool is_zero() const { return digits_.empty(); }

  // ---- magnitude-only primitives (ignore sign) -----------------------------
  static int cmp_mag(const std::vector<int> &a, const std::vector<int> &b);
  static std::vector<int> add_mag(const std::vector<int> &a,
                                  const std::vector<int> &b);
  // requires a >= b
  static std::vector<int> sub_mag(const std::vector<int> &a,
                                  const std::vector<int> &b);
  static std::vector<int> mul_mag(const std::vector<int> &a,
                                  const std::vector<int> &b);
  static std::vector<int> mul_small(const std::vector<int> &a, int m);
  static std::vector<int> div_small(const std::vector<int> &a, int d);
  // divmod on magnitudes: a = q * b + r with 0 <= r < b (b != 0)
  static void divmod_mag(const std::vector<int> &a, const std::vector<int> &b,
                         std::vector<int> &q, std::vector<int> &r);

  // fast multiplication helpers
  static std::vector<int> mul_naive(const std::vector<int> &a,
                                    const std::vector<int> &b);
  static std::vector<int> mul_fft(const std::vector<int> &a,
                                  const std::vector<int> &b);
  static void fft(std::vector<std::complex<double>> &a, bool invert);

  // division helpers (operate on non-negative magnitudes)
  // reciprocal(b, k) == floor(kBase^k / b), computed with Newton's method
  static std::vector<int> reciprocal(const std::vector<int> &b, int k);
  // Newton-reciprocal division; the base case used by the recursive divider.
  static void newton_divmod(const std::vector<int> &a, const std::vector<int> &b,
                            std::vector<int> &q, std::vector<int> &r);
  // Burnikel-Ziegler recursive division (b must be normalised, len(b) == n a
  // power of two). div_2n_1n / div_3n_2n are its two mutually recursive steps.
  static void bz_divmod(const std::vector<int> &a, const std::vector<int> &b,
                        int n, std::vector<int> &q, std::vector<int> &r);
  static void div_2n_1n(const std::vector<int> &a, const std::vector<int> &b,
                        int n, std::vector<int> &q, std::vector<int> &r);
  static void div_3n_2n(const std::vector<int> &a, const std::vector<int> &b,
                        int n, std::vector<int> &q, std::vector<int> &r);
  // floor division for signed values, remainder shares the sign of the divisor
  static void divmod_floor(const int2048 &a, const int2048 &b, int2048 &q,
                           int2048 &r);

  // build a signed value from a magnitude and a sign
  static int2048 from_mag(std::vector<int> mag, int sign);

public:
  // Constructors
  int2048();
  int2048(long long);
  int2048(const std::string &);
  int2048(const int2048 &);

  // The parameter types of the following functions are for reference only, you can choose to use constant references or not
  // If needed, you can add other required functions yourself
  // ===================================
  // Integer1
  // ===================================

  // Read a big integer
  void read(const std::string &);
  // Output the stored big integer, no need for newline
  void print();

  // Add a big integer
  int2048 &add(const int2048 &);
  // Return the sum of two big integers
  friend int2048 add(int2048, const int2048 &);

  // Subtract a big integer
  int2048 &minus(const int2048 &);
  // Return the difference of two big integers
  friend int2048 minus(int2048, const int2048 &);

  // ===================================
  // Integer2
  // ===================================

  int2048 operator+() const;
  int2048 operator-() const;

  int2048 &operator=(const int2048 &);

  int2048 &operator+=(const int2048 &);
  friend int2048 operator+(int2048, const int2048 &);

  int2048 &operator-=(const int2048 &);
  friend int2048 operator-(int2048, const int2048 &);

  int2048 &operator*=(const int2048 &);
  friend int2048 operator*(int2048, const int2048 &);

  int2048 &operator/=(const int2048 &);
  friend int2048 operator/(int2048, const int2048 &);

  int2048 &operator%=(const int2048 &);
  friend int2048 operator%(int2048, const int2048 &);

  friend std::istream &operator>>(std::istream &, int2048 &);
  friend std::ostream &operator<<(std::ostream &, const int2048 &);

  friend bool operator==(const int2048 &, const int2048 &);
  friend bool operator!=(const int2048 &, const int2048 &);
  friend bool operator<(const int2048 &, const int2048 &);
  friend bool operator>(const int2048 &, const int2048 &);
  friend bool operator<=(const int2048 &, const int2048 &);
  friend bool operator>=(const int2048 &, const int2048 &);
};
} // namespace sjtu

#endif
