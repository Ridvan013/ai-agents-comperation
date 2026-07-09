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

private:
  // Value is stored in base 10^8, little-endian (least significant limb first).
  // Invariant (normalized form): no trailing zero limbs; the value 0 is
  // represented by an empty `digits` vector with sign_ == 1.
  static const int kBase = 100000000;  // 10^8
  static const int kWidth = 8;         // decimal digits per limb

  std::vector<int> digits_;  // magnitude limbs, base 10^8, little-endian
  int sign_;                 // +1 or -1; always +1 when the value is zero

  // --- internal helpers -----------------------------------------------
  void normalize();                 // strip leading zero limbs, fix zero sign
  bool isZero() const;

  // magnitude-only comparison: -1 / 0 / 1
  static int cmpMag(const std::vector<int> &a, const std::vector<int> &b);
  // magnitude addition / subtraction (subtraction assumes a >= b)
  static std::vector<int> addMag(const std::vector<int> &a,
                                 const std::vector<int> &b);
  static std::vector<int> subMag(const std::vector<int> &a,
                                 const std::vector<int> &b);
  // magnitude multiplication (schoolbook for small, FFT for large)
  static std::vector<int> mulMag(const std::vector<int> &a,
                                 const std::vector<int> &b);
  // magnitude floor division: q = floor(a / b), r = a - q*b (b != 0)
  static void divModMag(const std::vector<int> &a, const std::vector<int> &b,
                        std::vector<int> &q, std::vector<int> &r);

  // FFT used by mulMag for large operands
  static void fft(std::vector<std::complex<double>> &v, bool invert);
};
}  // namespace sjtu

#endif
