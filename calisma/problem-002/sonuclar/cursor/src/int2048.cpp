#include "int2048.h"

#include <algorithm>
#include <string>

namespace sjtu {

// ======================================================================
// Construction / assignment
// ======================================================================

int2048::int2048() : sign_(1) {}

int2048::int2048(long long x) : sign_(1) {
  if (x < 0) {
    sign_ = -1;
  }
  // Use unsigned to safely handle LLONG_MIN.
  unsigned long long v =
      (x < 0) ? (~static_cast<unsigned long long>(x) + 1ULL)
              : static_cast<unsigned long long>(x);
  while (v > 0) {
    digits_.push_back(static_cast<int>(v % kBase));
    v /= kBase;
  }
  normalize();
}

int2048::int2048(const std::string &s) : sign_(1) { read(s); }

int2048::int2048(const int2048 &other) = default;

int2048 &int2048::operator=(const int2048 &other) = default;

// ======================================================================
// Internal helpers
// ======================================================================

void int2048::normalize() {
  while (!digits_.empty() && digits_.back() == 0) digits_.pop_back();
  if (digits_.empty()) sign_ = 1;  // canonical zero
}

bool int2048::isZero() const { return digits_.empty(); }

int int2048::cmpMag(const std::vector<int> &a, const std::vector<int> &b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  }
  return 0;
}

std::vector<int> int2048::addMag(const std::vector<int> &a,
                                 const std::vector<int> &b) {
  std::vector<int> res;
  res.reserve(std::max(a.size(), b.size()) + 1);
  int carry = 0;
  for (size_t i = 0; i < a.size() || i < b.size() || carry; ++i) {
    int cur = carry;
    if (i < a.size()) cur += a[i];
    if (i < b.size()) cur += b[i];
    if (cur >= kBase) {
      carry = 1;
      cur -= kBase;
    } else {
      carry = 0;
    }
    res.push_back(cur);
  }
  return res;
}

// Requires cmpMag(a, b) >= 0.
std::vector<int> int2048::subMag(const std::vector<int> &a,
                                 const std::vector<int> &b) {
  std::vector<int> res;
  res.reserve(a.size());
  int borrow = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    int cur = a[i] - borrow - (i < b.size() ? b[i] : 0);
    if (cur < 0) {
      cur += kBase;
      borrow = 1;
    } else {
      borrow = 0;
    }
    res.push_back(cur);
  }
  while (!res.empty() && res.back() == 0) res.pop_back();
  return res;
}

// ======================================================================
// FFT-based multiplication
// ======================================================================

void int2048::fft(std::vector<std::complex<double>> &v, bool invert) {
  const int n = static_cast<int>(v.size());
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(v[i], v[j]);
  }
  const long double PI = 3.14159265358979323846264338327950288L;
  // For each level, precompute the twiddle factors directly from trig
  // functions instead of accumulating with repeated multiplication. This
  // keeps rounding error tiny even for very large transforms.
  std::vector<std::complex<double>> w;
  for (int len = 2; len <= n; len <<= 1) {
    const int half = len >> 1;
    const long double ang = 2 * PI / len * (invert ? -1 : 1);
    w.resize(half);
    for (int k = 0; k < half; ++k) {
      w[k] = std::complex<double>(static_cast<double>(std::cos(ang * k)),
                                  static_cast<double>(std::sin(ang * k)));
    }
    for (int i = 0; i < n; i += len) {
      for (int k = 0; k < half; ++k) {
        std::complex<double> u = v[i + k];
        std::complex<double> t = v[i + k + half] * w[k];
        v[i + k] = u + t;
        v[i + k + half] = u - t;
      }
    }
  }
  if (invert) {
    for (auto &x : v) x /= n;
  }
}

std::vector<int> int2048::mulMag(const std::vector<int> &a,
                                 const std::vector<int> &b) {
  if (a.empty() || b.empty()) return {};

  const size_t na = a.size(), nb = b.size();

  // Schoolbook multiplication for small operands (avoids FFT overhead).
  if (na <= 64 || nb <= 64) {
    std::vector<long long> tmp(na + nb, 0);
    for (size_t i = 0; i < na; ++i) {
      long long carry = 0;
      long long ai = a[i];
      for (size_t j = 0; j < nb; ++j) {
        long long cur = tmp[i + j] + ai * b[j] + carry;
        tmp[i + j] = cur % kBase;
        carry = cur / kBase;
      }
      tmp[i + nb] += carry;
    }
    std::vector<int> res(na + nb);
    long long carry = 0;
    for (size_t i = 0; i < tmp.size(); ++i) {
      long long cur = tmp[i] + carry;
      res[i] = static_cast<int>(cur % kBase);
      carry = cur / kBase;
    }
    while (!res.empty() && res.back() == 0) res.pop_back();
    return res;
  }

  // FFT multiplication: split each base-10^8 limb into two base-10^4 halves
  // so that coefficients stay small enough for double precision.
  std::vector<int> A;
  A.reserve(na * 2);
  for (size_t i = 0; i < na; ++i) {
    A.push_back(a[i] % 10000);
    A.push_back(a[i] / 10000);
  }
  std::vector<int> B;
  B.reserve(nb * 2);
  for (size_t i = 0; i < nb; ++i) {
    B.push_back(b[i] % 10000);
    B.push_back(b[i] / 10000);
  }

  int resultSize = static_cast<int>(A.size() + B.size());
  int n = 1;
  while (n < resultSize) n <<= 1;

  std::vector<std::complex<double>> fa(n), fb(n);
  for (size_t i = 0; i < A.size(); ++i) fa[i] = A[i];
  for (size_t i = 0; i < B.size(); ++i) fb[i] = B[i];

  fft(fa, false);
  fft(fb, false);
  for (int i = 0; i < n; ++i) fa[i] *= fb[i];
  fft(fa, true);

  // Recombine into base 10^4 with carries.
  std::vector<long long> c(n);
  for (int i = 0; i < n; ++i) {
    c[i] = static_cast<long long>(fa[i].real() + 0.5);
  }
  long long carry = 0;
  for (int i = 0; i < n; ++i) {
    long long cur = c[i] + carry;
    c[i] = cur % 10000;
    carry = cur / 10000;
  }
  // (carry should be 0 here because n is large enough)

  // Merge base-10^4 pairs back into base-10^8 limbs.
  std::vector<int> res;
  res.reserve(na + nb + 1);
  for (int i = 0; i + 1 < n; i += 2) {
    res.push_back(static_cast<int>(c[i] + c[i + 1] * 10000));
  }
  if (n % 2 == 1) res.push_back(static_cast<int>(c[n - 1]));
  while (!res.empty() && res.back() == 0) res.pop_back();
  return res;
}

// ======================================================================
// Magnitude floor division (Knuth Algorithm D), base 10^8
// ======================================================================

void int2048::divModMag(const std::vector<int> &a, const std::vector<int> &b,
                        std::vector<int> &q, std::vector<int> &r) {
  q.clear();
  r.clear();
  if (cmpMag(a, b) < 0) {  // a < b  => quotient 0, remainder a
    r = a;
    return;
  }

  const int m = static_cast<int>(b.size());

  // Single-limb divisor: fast path.
  if (m == 1) {
    long long divisor = b[0];
    long long rem = 0;
    q.assign(a.size(), 0);
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
      long long cur = rem * kBase + a[i];
      q[i] = static_cast<int>(cur / divisor);
      rem = cur % divisor;
    }
    while (!q.empty() && q.back() == 0) q.pop_back();
    if (rem) r.push_back(static_cast<int>(rem));
    return;
  }

  // Normalize so that the leading limb of the divisor is >= kBase/2.
  const long long d = kBase / (static_cast<long long>(b.back()) + 1);
  std::vector<int> u;  // normalized dividend, gets an extra high limb
  std::vector<int> v;  // normalized divisor
  if (d == 1) {
    u = a;
    u.push_back(0);
    v = b;
  } else {
    // u = a * d
    long long carry = 0;
    u.resize(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
      long long cur = static_cast<long long>(a[i]) * d + carry;
      u[i] = static_cast<int>(cur % kBase);
      carry = cur / kBase;
    }
    u.push_back(static_cast<int>(carry));
    // v = b * d
    carry = 0;
    v.resize(b.size());
    for (size_t i = 0; i < b.size(); ++i) {
      long long cur = static_cast<long long>(b[i]) * d + carry;
      v[i] = static_cast<int>(cur % kBase);
      carry = cur / kBase;
    }
    // v.back() carry is guaranteed 0 by choice of d.
  }

  const int n = static_cast<int>(a.size());  // dividend limb count
  q.assign(n - m + 1, 0);

  const long long vTop = v[m - 1];
  const long long vSecond = v[m - 2];

  for (int j = n - m; j >= 0; --j) {
    // Estimate quotient limb qhat.
    long long num =
        static_cast<long long>(u[j + m]) * kBase + u[j + m - 1];
    long long qhat = num / vTop;
    long long rhat = num % vTop;

    while (qhat >= kBase ||
           qhat * vSecond > rhat * kBase + u[j + m - 2]) {
      --qhat;
      rhat += vTop;
      if (rhat >= kBase) break;
    }

    // Multiply and subtract: u[j..j+m] -= qhat * v[0..m-1].
    long long borrow = 0, carry = 0;
    for (int i = 0; i < m; ++i) {
      long long p = qhat * v[i] + carry;
      carry = p / kBase;
      long long sub = static_cast<long long>(u[j + i]) - (p % kBase) - borrow;
      if (sub < 0) {
        sub += kBase;
        borrow = 1;
      } else {
        borrow = 0;
      }
      u[j + i] = static_cast<int>(sub);
    }
    long long sub = static_cast<long long>(u[j + m]) - carry - borrow;
    if (sub < 0) {
      sub += kBase;
      borrow = 1;
    } else {
      borrow = 0;
    }
    u[j + m] = static_cast<int>(sub);

    // If we subtracted too much, add back one multiple of v.
    if (borrow) {
      --qhat;
      long long c = 0;
      for (int i = 0; i < m; ++i) {
        long long cur = static_cast<long long>(u[j + i]) + v[i] + c;
        if (cur >= kBase) {
          cur -= kBase;
          c = 1;
        } else {
          c = 0;
        }
        u[j + i] = static_cast<int>(cur);
      }
      u[j + m] = static_cast<int>(u[j + m] + c);  // discards final carry
    }

    q[j] = static_cast<int>(qhat);
  }

  while (!q.empty() && q.back() == 0) q.pop_back();

  // Remainder = (normalized remainder in u) / d.
  u.resize(m);
  if (d == 1) {
    r = u;
  } else {
    r.assign(m, 0);
    long long rem = 0;
    for (int i = m - 1; i >= 0; --i) {
      long long cur = rem * kBase + u[i];
      r[i] = static_cast<int>(cur / d);
      rem = cur % d;
    }
  }
  while (!r.empty() && r.back() == 0) r.pop_back();
}

// ======================================================================
// I/O
// ======================================================================

void int2048::read(const std::string &s) {
  digits_.clear();
  sign_ = 1;

  size_t pos = 0;
  const size_t len = s.size();
  while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' ||
                       s[pos] == '\r')) {
    ++pos;
  }
  if (pos < len && (s[pos] == '+' || s[pos] == '-')) {
    if (s[pos] == '-') sign_ = -1;
    ++pos;
  }
  // Skip leading zeros (but remember where digits end).
  size_t start = pos;
  while (start < len && s[start] == '0') ++start;

  // Find end of the digit run.
  size_t end = start;
  while (end < len && s[end] >= '0' && s[end] <= '9') ++end;

  if (start == end) {  // value is zero
    sign_ = 1;
    return;
  }

  // Parse from least significant side in chunks of kWidth digits.
  for (int i = static_cast<int>(end); i > static_cast<int>(start);
       i -= kWidth) {
    int lo = std::max(static_cast<int>(start), i - kWidth);
    int cur = 0;
    for (int k = lo; k < i; ++k) cur = cur * 10 + (s[k] - '0');
    digits_.push_back(cur);
  }
  normalize();
}

void int2048::print() { std::cout << *this; }

std::istream &operator>>(std::istream &is, int2048 &x) {
  std::string s;
  is >> s;
  x.read(s);
  return is;
}

std::ostream &operator<<(std::ostream &os, const int2048 &x) {
  if (x.digits_.empty()) {
    os << '0';
    return os;
  }
  if (x.sign_ < 0) os << '-';
  os << x.digits_.back();
  char buf[16];
  for (int i = static_cast<int>(x.digits_.size()) - 2; i >= 0; --i) {
    std::snprintf(buf, sizeof(buf), "%08d", x.digits_[i]);
    os << buf;
  }
  return os;
}

// ======================================================================
// Addition / subtraction (signed)
// ======================================================================

int2048 &int2048::add(const int2048 &rhs) {
  if (sign_ == rhs.sign_) {
    digits_ = addMag(digits_, rhs.digits_);
  } else {
    int c = cmpMag(digits_, rhs.digits_);
    if (c == 0) {
      digits_.clear();
      sign_ = 1;
      return *this;
    }
    if (c > 0) {
      digits_ = subMag(digits_, rhs.digits_);
      // sign_ stays
    } else {
      digits_ = subMag(rhs.digits_, digits_);
      sign_ = rhs.sign_;
    }
  }
  normalize();
  return *this;
}

int2048 add(int2048 a, const int2048 &b) { return a.add(b); }

int2048 &int2048::minus(const int2048 &rhs) {
  // a - b == a + (-b): flip rhs sign logically without copying when possible.
  if (rhs.isZero()) return *this;
  if (isZero()) {
    digits_ = rhs.digits_;
    sign_ = -rhs.sign_;
    normalize();
    return *this;
  }
  if (sign_ != rhs.sign_) {
    // different signs -> magnitudes add
    digits_ = addMag(digits_, rhs.digits_);
    normalize();
    return *this;
  }
  // same sign -> magnitudes subtract
  int c = cmpMag(digits_, rhs.digits_);
  if (c == 0) {
    digits_.clear();
    sign_ = 1;
    return *this;
  }
  if (c > 0) {
    digits_ = subMag(digits_, rhs.digits_);
  } else {
    digits_ = subMag(rhs.digits_, digits_);
    sign_ = -sign_;
  }
  normalize();
  return *this;
}

int2048 minus(int2048 a, const int2048 &b) { return a.minus(b); }

// ======================================================================
// Unary operators
// ======================================================================

int2048 int2048::operator+() const { return *this; }

int2048 int2048::operator-() const {
  int2048 res(*this);
  if (!res.isZero()) res.sign_ = -res.sign_;
  return res;
}

// ======================================================================
// Compound assignment + binary arithmetic
// ======================================================================

int2048 &int2048::operator+=(const int2048 &rhs) { return add(rhs); }
int2048 operator+(int2048 a, const int2048 &b) { return a += b; }

int2048 &int2048::operator-=(const int2048 &rhs) { return minus(rhs); }
int2048 operator-(int2048 a, const int2048 &b) { return a -= b; }

int2048 &int2048::operator*=(const int2048 &rhs) {
  if (isZero() || rhs.isZero()) {
    digits_.clear();
    sign_ = 1;
    return *this;
  }
  digits_ = mulMag(digits_, rhs.digits_);
  sign_ = sign_ * rhs.sign_;
  normalize();
  return *this;
}
int2048 operator*(int2048 a, const int2048 &b) { return a *= b; }

int2048 &int2048::operator/=(const int2048 &rhs) {
  // Floor division (rounds toward negative infinity).
  std::vector<int> q, r;
  divModMag(digits_, rhs.digits_, q, r);
  int resSign = sign_ * rhs.sign_;
  bool remNonZero = !r.empty();

  // Adjust toward negative infinity: when the true quotient is negative and
  // the division is inexact, the truncated magnitude must be increased by 1.
  if (resSign < 0 && remNonZero) q = addMag(q, {1});

  digits_ = std::move(q);
  sign_ = resSign;
  normalize();
  return *this;
}
int2048 operator/(int2048 a, const int2048 &b) { return a /= b; }

int2048 &int2048::operator%=(const int2048 &rhs) {
  // x % y = x - (x / y) * y  (consistent with floor division).
  std::vector<int> q, r;
  divModMag(digits_, rhs.digits_, q, r);

  if (r.empty()) {  // exact division -> remainder 0
    digits_.clear();
    sign_ = 1;
    return *this;
  }

  // Truncated remainder has the sign of the dividend (this->sign_).
  // Floor remainder must have the sign of the divisor.
  if (sign_ == rhs.sign_) {
    // signs equal -> truncated remainder already matches divisor sign
    digits_ = std::move(r);
    // sign_ stays (== dividend sign == divisor sign)
  } else {
    // floor remainder = truncated_remainder + divisor
    // truncated remainder magnitude = r with dividend sign;
    // result = (r with dividend sign) + rhs
    // Compute directly: rhs.mag - r.mag, sign = rhs.sign_ (since |r| < |rhs|)
    digits_ = subMag(rhs.digits_, r);
    sign_ = rhs.sign_;
  }
  normalize();
  return *this;
}
int2048 operator%(int2048 a, const int2048 &b) { return a %= b; }

// ======================================================================
// Comparison operators
// ======================================================================

bool operator==(const int2048 &a, const int2048 &b) {
  return a.sign_ == b.sign_ && a.digits_ == b.digits_;
}
bool operator!=(const int2048 &a, const int2048 &b) { return !(a == b); }

bool operator<(const int2048 &a, const int2048 &b) {
  if (a.sign_ != b.sign_) return a.sign_ < b.sign_;
  int c = int2048::cmpMag(a.digits_, b.digits_);
  return a.sign_ > 0 ? c < 0 : c > 0;
}
bool operator>(const int2048 &a, const int2048 &b) { return b < a; }
bool operator<=(const int2048 &a, const int2048 &b) { return !(b < a); }
bool operator>=(const int2048 &a, const int2048 &b) { return !(a < b); }

}  // namespace sjtu
