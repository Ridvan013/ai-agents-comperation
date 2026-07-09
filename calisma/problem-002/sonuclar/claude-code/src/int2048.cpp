#include "int2048.h"

namespace sjtu {

namespace {
// Small helpers so that the implementation stays within the headers allowed by
// int2048.h (no <algorithm>, <cmath> or <string> beyond what the interface
// already relies on).
inline size_t max_size(size_t a, size_t b) { return a > b ? a : b; }
inline size_t min_size(size_t a, size_t b) { return a < b ? a : b; }
inline long long round_to_ll(double x) {
  return static_cast<long long>(x + (x >= 0 ? 0.5 : -0.5));
}
} // namespace

// ===========================================================================
// Canonicalisation
// ===========================================================================

void int2048::trim() {
  while (!digits_.empty() && digits_.back() == 0)
    digits_.pop_back();
  if (digits_.empty())
    sign_ = 1; // zero is always non-negative
}

int2048 int2048::from_mag(std::vector<int> mag, int sign) {
  int2048 res;
  res.digits_ = std::move(mag);
  res.sign_ = sign;
  res.trim();
  return res;
}

// ===========================================================================
// Magnitude primitives
// ===========================================================================

int int2048::cmp_mag(const std::vector<int> &a, const std::vector<int> &b) {
  if (a.size() != b.size())
    return a.size() < b.size() ? -1 : 1;
  for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i)
    if (a[i] != b[i])
      return a[i] < b[i] ? -1 : 1;
  return 0;
}

std::vector<int> int2048::add_mag(const std::vector<int> &a,
                                  const std::vector<int> &b) {
  std::vector<int> res;
  res.reserve(max_size(a.size(), b.size()) + 1);
  int carry = 0;
  for (size_t i = 0; i < a.size() || i < b.size() || carry; ++i) {
    int cur = carry;
    if (i < a.size())
      cur += a[i];
    if (i < b.size())
      cur += b[i];
    res.push_back(cur % kBase);
    carry = cur / kBase;
  }
  return res;
}

// requires cmp_mag(a, b) >= 0
std::vector<int> int2048::sub_mag(const std::vector<int> &a,
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
  while (!res.empty() && res.back() == 0)
    res.pop_back();
  return res;
}

// ===========================================================================
// Multiplication
// ===========================================================================

std::vector<int> int2048::mul_naive(const std::vector<int> &a,
                                    const std::vector<int> &b) {
  if (a.empty() || b.empty())
    return {};
  std::vector<long long> tmp(a.size() + b.size(), 0);
  for (size_t i = 0; i < a.size(); ++i) {
    long long carry = 0;
    long long ai = a[i];
    for (size_t j = 0; j < b.size(); ++j) {
      long long cur = tmp[i + j] + ai * b[j] + carry;
      tmp[i + j] = cur % kBase;
      carry = cur / kBase;
    }
    tmp[i + b.size()] += carry;
  }
  std::vector<int> res(tmp.size());
  long long carry = 0;
  for (size_t i = 0; i < tmp.size(); ++i) {
    long long cur = tmp[i] + carry;
    res[i] = static_cast<int>(cur % kBase);
    carry = cur / kBase;
  }
  while (!res.empty() && res.back() == 0)
    res.pop_back();
  return res;
}

// In-place iterative Cooley-Tukey FFT.
void int2048::fft(std::vector<std::complex<double>> &a, bool invert) {
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  const double PI = 3.14159265358979323846;
  for (size_t len = 2; len <= n; len <<= 1) {
    double ang = 2 * PI / static_cast<double>(len) * (invert ? -1 : 1);
    std::complex<double> wlen = std::polar(1.0, ang);
    for (size_t i = 0; i < n; i += len) {
      std::complex<double> w(1.0, 0.0);
      for (size_t k = 0; k < len / 2; ++k) {
        std::complex<double> u = a[i + k];
        std::complex<double> v = a[i + k + len / 2] * w;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
  if (invert)
    for (auto &x : a)
      x /= static_cast<double>(n);
}

std::vector<int> int2048::mul_fft(const std::vector<int> &a,
                                  const std::vector<int> &b) {
  if (a.empty() || b.empty())
    return {};
  size_t need = a.size() + b.size();
  size_t n = 1;
  while (n < need)
    n <<= 1;

  // Pack a into the real part and b into the imaginary part of one array so
  // that a single forward transform yields the transforms of both inputs.
  std::vector<std::complex<double>> fa(n);
  for (size_t i = 0; i < a.size(); ++i)
    fa[i].real(static_cast<double>(a[i]));
  for (size_t i = 0; i < b.size(); ++i)
    fa[i].imag(static_cast<double>(b[i]));

  fft(fa, false);

  // Recover the pointwise product of transform(a) and transform(b).
  std::vector<std::complex<double>> fc(n);
  for (size_t k = 0; k < n; ++k) {
    size_t j = (n - k) & (n - 1);
    std::complex<double> A = (fa[k] + std::conj(fa[j])) * 0.5;
    std::complex<double> B =
        (fa[k] - std::conj(fa[j])) * std::complex<double>(0.0, -0.5);
    fc[k] = A * B;
  }

  fft(fc, true);

  std::vector<int> res(need, 0);
  long long carry = 0;
  for (size_t i = 0; i < need; ++i) {
    long long cur = carry + round_to_ll(fc[i].real());
    res[i] = static_cast<int>(cur % kBase);
    carry = cur / kBase;
  }
  while (carry) {
    res.push_back(static_cast<int>(carry % kBase));
    carry /= kBase;
  }
  while (!res.empty() && res.back() == 0)
    res.pop_back();
  return res;
}

std::vector<int> int2048::mul_mag(const std::vector<int> &a,
                                  const std::vector<int> &b) {
  if (a.empty() || b.empty())
    return {};
  // Schoolbook multiplication wins for small operands and avoids any risk of
  // floating-point rounding error; the FFT path takes over once it pays off.
  if (min_size(a.size(), b.size()) <= 64)
    return mul_naive(a, b);
  return mul_fft(a, b);
}

// ===========================================================================
// Division
// ===========================================================================

namespace {
// The next three helpers only shuffle limbs, so they need no knowledge of the
// base and live outside the class.

// Drop the k least-significant limbs (an integer division by kBase^k).
std::vector<int> shift_right(const std::vector<int> &a, int k) {
  if (k >= static_cast<int>(a.size()))
    return {};
  return std::vector<int>(a.begin() + k, a.end());
}

// Prepend k zero limbs (a multiplication by kBase^k).
std::vector<int> shift_left(const std::vector<int> &a, int k) {
  if (a.empty())
    return {};
  std::vector<int> r(a.size() + k, 0);
  for (size_t i = 0; i < a.size(); ++i)
    r[i + k] = a[i];
  return r;
}

// The `len` limbs starting at `lo`, zero-padded and trimmed.
std::vector<int> slice(const std::vector<int> &a, int lo, int len) {
  std::vector<int> r(len, 0);
  int n = static_cast<int>(a.size());
  for (int i = 0; i < len && lo + i < n; ++i)
    r[i] = a[lo + i];
  while (!r.empty() && r.back() == 0)
    r.pop_back();
  return r;
}
} // namespace

std::vector<int> int2048::mul_small(const std::vector<int> &a, int m) {
  if (a.empty() || m == 0)
    return {};
  std::vector<int> r;
  r.reserve(a.size() + 1);
  long long carry = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    long long cur = static_cast<long long>(a[i]) * m + carry;
    r.push_back(static_cast<int>(cur % kBase));
    carry = cur / kBase;
  }
  while (carry) {
    r.push_back(static_cast<int>(carry % kBase));
    carry /= kBase;
  }
  return r;
}

std::vector<int> int2048::div_small(const std::vector<int> &a, int d) {
  std::vector<int> q(a.size(), 0);
  long long rem = 0;
  for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
    long long cur = rem * kBase + a[i];
    q[i] = static_cast<int>(cur / d);
    rem = cur % d;
  }
  while (!q.empty() && q.back() == 0)
    q.pop_back();
  return q;
}

std::vector<int> int2048::reciprocal(const std::vector<int> &b, int k) {
  const int n = static_cast<int>(b.size());

  // Initial estimate from the two leading limbs of b. Because those limbs form
  // a lower bound on b, the estimate over-approximates the true reciprocal by
  // at most a factor of (1 + 1/kBase) -- a good seed for Newton's method.
  long long top = static_cast<long long>(b[n - 1]) * kBase + b[n - 2];
  int m = k - n + 2; // scale of the seed: floor(kBase^m / top)
  std::vector<int> r;
  {
    // Long division of kBase^m by the scalar `top`.
    r.assign(m + 1, 0);
    long long rem = 0;
    for (int i = m; i >= 0; --i) {
      long long cur = rem * kBase + (i == m ? 1 : 0);
      r[i] = static_cast<int>(cur / top);
      rem = cur % top;
    }
    while (!r.empty() && r.back() == 0)
      r.pop_back();
  }

  // two_scaled == 2 * kBase^k
  std::vector<int> two_scaled(k + 1, 0);
  two_scaled[k] = 2;

  // Newton iteration: r <- floor(r * (2*kBase^k - b*r) / kBase^k), which
  // doubles the number of correct limbs each round.
  for (int iter = 0; iter < 100; ++iter) {
    std::vector<int> p = mul_mag(b, r); // b * r  (stays below 2*kBase^k)
    std::vector<int> d = sub_mag(two_scaled, p);
    std::vector<int> nr = shift_right(mul_mag(r, d), k);
    while (!nr.empty() && nr.back() == 0)
      nr.pop_back();
    if (cmp_mag(nr, r) == 0)
      break;
    r = std::move(nr);
  }
  return r;
}

void int2048::newton_divmod(const std::vector<int> &a,
                            const std::vector<int> &b, std::vector<int> &q,
                            std::vector<int> &r) {
  if (cmp_mag(a, b) < 0) {
    q.clear();
    r = a;
    return;
  }
  // Single-limb divisor: a plain long division is fastest and exact.
  if (b.size() == 1) {
    long long d = b[0];
    q.assign(a.size(), 0);
    long long rem = 0;
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
      long long cur = rem * kBase + a[i];
      q[i] = static_cast<int>(cur / d);
      rem = cur % d;
    }
    while (!q.empty() && q.back() == 0)
      q.pop_back();
    r.clear();
    if (rem)
      r.push_back(static_cast<int>(rem));
    return;
  }

  // Multiply by a Newton reciprocal of b, then correct the estimate.
  int k = static_cast<int>(a.size());
  std::vector<int> inv = reciprocal(b, k);
  q = shift_right(mul_mag(a, inv), k);
  while (!q.empty() && q.back() == 0)
    q.pop_back();

  std::vector<int> prod = mul_mag(q, b);
  const std::vector<int> one{1};
  // The estimate can be a little too large ...
  while (cmp_mag(prod, a) > 0) {
    q = sub_mag(q, one);
    prod = sub_mag(prod, b);
  }
  r = sub_mag(a, prod);
  // ... or a little too small.
  while (cmp_mag(r, b) >= 0) {
    q = add_mag(q, one);
    r = sub_mag(r, b);
  }
}

// Below this many limbs, plain Newton division is already the fastest option
// and the recursion bottoms out.
static const int kBZThreshold = 32;

void int2048::div_3n_2n(const std::vector<int> &a, const std::vector<int> &b,
                        int n, std::vector<int> &q, std::vector<int> &r) {
  int half = n / 2;
  std::vector<int> b1 = slice(b, half, half); // high half of the divisor
  std::vector<int> b0 = slice(b, 0, half);    // low half of the divisor
  std::vector<int> a2 = slice(a, 2 * half, half);
  std::vector<int> a12 = slice(a, half, n); // top two half-blocks of a
  std::vector<int> a0 = slice(a, 0, half);

  std::vector<int> qhat, r1;
  if (cmp_mag(a2, b1) < 0) {
    div_2n_1n(a12, b1, half, qhat, r1);
  } else {
    // The quotient would overflow half a block; clamp it and adjust.
    qhat.assign(half, kBase - 1); // kBase^half - 1
    r1 = add_mag(sub_mag(a12, shift_left(b1, half)), b1);
  }

  std::vector<int> d = mul_mag(qhat, b0);
  std::vector<int> rem = add_mag(shift_left(r1, half), a0);
  // At most two corrections are needed because b is normalised.
  const std::vector<int> one{1};
  while (cmp_mag(rem, d) < 0) {
    qhat = sub_mag(qhat, one);
    rem = add_mag(rem, b);
  }
  q = qhat;
  r = sub_mag(rem, d);
}

void int2048::div_2n_1n(const std::vector<int> &a, const std::vector<int> &b,
                        int n, std::vector<int> &q, std::vector<int> &r) {
  if (n <= kBZThreshold) {
    newton_divmod(a, b, q, r);
    return;
  }
  int half = n / 2;
  std::vector<int> a_high = slice(a, half, 3 * half); // top three half-blocks
  std::vector<int> a0 = slice(a, 0, half);

  std::vector<int> q1, r1;
  div_3n_2n(a_high, b, n, q1, r1);
  std::vector<int> a2 = add_mag(shift_left(r1, half), a0);
  std::vector<int> q0, r0;
  div_3n_2n(a2, b, n, q0, r0);

  q = add_mag(shift_left(q1, half), q0);
  r = r0;
}

void int2048::bz_divmod(const std::vector<int> &a, const std::vector<int> &b,
                        int n, std::vector<int> &q, std::vector<int> &r) {
  int m = static_cast<int>(a.size());
  int t = (m + n - 1) / n;
  if (t < 1)
    t = 1;
  // Make sure the most significant block is smaller than b so that every
  // div_2n_1n call meets its precondition (dividend < b * kBase^n).
  if (cmp_mag(slice(a, (t - 1) * n, n), b) >= 0)
    ++t;

  std::vector<int> rem = slice(a, (t - 1) * n, n);
  std::vector<int> quo(static_cast<size_t>(t - 1) * n, 0);
  for (int i = t - 2; i >= 0; --i) {
    std::vector<int> cur = add_mag(shift_left(rem, n), slice(a, i * n, n));
    std::vector<int> qi, ri;
    div_2n_1n(cur, b, n, qi, ri);
    for (size_t j = 0; j < qi.size(); ++j)
      quo[static_cast<size_t>(i) * n + j] = qi[j];
    rem = ri;
  }
  while (!quo.empty() && quo.back() == 0)
    quo.pop_back();
  q = std::move(quo);
  r = std::move(rem);
}

void int2048::divmod_mag(const std::vector<int> &a, const std::vector<int> &b,
                         std::vector<int> &q, std::vector<int> &r) {
  if (cmp_mag(a, b) < 0) {
    q.clear();
    r = a;
    return;
  }
  // Small operands: the plain Newton divider avoids all recursion overhead.
  if (b.size() == 1 || a.size() <= 64) {
    newton_divmod(a, b, q, r);
    return;
  }

  // Normalise so that the divisor's leading limb is at least kBase / 2, then
  // pad the divisor length to a power of two -- both are preconditions of the
  // Burnikel-Ziegler recursion. The same scaling is undone on the remainder.
  int f = kBase / (b.back() + 1);
  std::vector<int> na = (f > 1) ? mul_small(a, f) : a;
  std::vector<int> nb = (f > 1) ? mul_small(b, f) : b;
  int n = static_cast<int>(nb.size());
  int N = kBZThreshold;
  while (N < n)
    N <<= 1;
  int z = N - n;
  if (z > 0) {
    na = shift_left(na, z);
    nb = shift_left(nb, z);
  }

  std::vector<int> rem;
  bz_divmod(na, nb, N, q, rem);
  // rem == (a mod b) * f * kBase^z, so peel both factors back off.
  if (z > 0)
    rem = shift_right(rem, z);
  r = (f > 1) ? div_small(rem, f) : rem;
}

void int2048::divmod_floor(const int2048 &a, const int2048 &b, int2048 &q,
                           int2048 &r) {
  if (a.is_zero()) {
    q = int2048();
    r = int2048();
    return;
  }
  std::vector<int> qm, rm;
  divmod_mag(a.digits_, b.digits_, qm, rm);
  int s = a.sign_ * b.sign_;
  if (rm.empty()) {
    q = from_mag(std::move(qm), s);
    r = int2048();
    return;
  }
  if (s > 0) {
    q = from_mag(std::move(qm), s);
    r = from_mag(std::move(rm), b.sign_);
  } else {
    // Rounding toward negative infinity: the truncated quotient magnitude
    // grows by one, and the remainder takes the sign of the divisor.
    q = from_mag(add_mag(qm, std::vector<int>{1}), -1);
    r = from_mag(sub_mag(b.digits_, rm), b.sign_);
  }
}

// ===========================================================================
// Constructors
// ===========================================================================

int2048::int2048() : sign_(1) {}

int2048::int2048(long long x) {
  sign_ = 1;
  unsigned long long v;
  if (x < 0) {
    sign_ = -1;
    v = static_cast<unsigned long long>(-(x + 1)) + 1ULL; // safe for LLONG_MIN
  } else {
    v = static_cast<unsigned long long>(x);
  }
  while (v) {
    digits_.push_back(static_cast<int>(v % kBase));
    v /= kBase;
  }
  trim();
}

int2048::int2048(const std::string &s) { read(s); }

int2048::int2048(const int2048 &other) = default;

// ===========================================================================
// I/O
// ===========================================================================

void int2048::read(const std::string &s) {
  digits_.clear();
  sign_ = 1;
  size_t pos = 0;
  while (pos < s.size() &&
         (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
    ++pos;
  if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
    if (s[pos] == '-')
      sign_ = -1;
    ++pos;
  }
  // Isolate the contiguous run of decimal digits.
  size_t start = pos;
  size_t end = pos;
  while (end < s.size() && s[end] >= '0' && s[end] <= '9')
    ++end;
  // Pack the digits kWidth at a time, starting from the least significant end.
  for (int i = static_cast<int>(end) - 1; i >= static_cast<int>(start);
       i -= kWidth) {
    int limb = 0;
    int place = 1;
    for (int j = 0; j < kWidth && i - j >= static_cast<int>(start); ++j) {
      limb += (s[i - j] - '0') * place;
      place *= 10;
    }
    digits_.push_back(limb);
  }
  trim();
}

void int2048::print() { std::cout << *this; }

// ===========================================================================
// Integer 1 interface
// ===========================================================================

int2048 &int2048::add(const int2048 &rhs) { return *this += rhs; }

int2048 add(int2048 a, const int2048 &b) { return a += b; }

int2048 &int2048::minus(const int2048 &rhs) { return *this -= rhs; }

int2048 minus(int2048 a, const int2048 &b) { return a -= b; }

// ===========================================================================
// Unary and assignment operators
// ===========================================================================

int2048 int2048::operator+() const { return *this; }

int2048 int2048::operator-() const {
  int2048 res(*this);
  if (!res.is_zero())
    res.sign_ = -res.sign_;
  return res;
}

int2048 &int2048::operator=(const int2048 &other) = default;

// ===========================================================================
// Addition and subtraction
// ===========================================================================

int2048 &int2048::operator+=(const int2048 &rhs) {
  if (sign_ == rhs.sign_) {
    digits_ = add_mag(digits_, rhs.digits_);
  } else {
    int c = cmp_mag(digits_, rhs.digits_);
    if (c == 0) {
      digits_.clear();
      sign_ = 1;
    } else if (c > 0) {
      digits_ = sub_mag(digits_, rhs.digits_);
    } else {
      digits_ = sub_mag(rhs.digits_, digits_);
      sign_ = rhs.sign_;
    }
  }
  trim();
  return *this;
}

int2048 operator+(int2048 a, const int2048 &b) { return a += b; }

int2048 &int2048::operator-=(const int2048 &rhs) {
  // a - b == a + (-b); mirror operator+= with the right operand's sign flipped.
  int rsign = rhs.is_zero() ? rhs.sign_ : -rhs.sign_;
  if (sign_ == rsign) {
    digits_ = add_mag(digits_, rhs.digits_);
  } else {
    int c = cmp_mag(digits_, rhs.digits_);
    if (c == 0) {
      digits_.clear();
      sign_ = 1;
    } else if (c > 0) {
      digits_ = sub_mag(digits_, rhs.digits_);
    } else {
      digits_ = sub_mag(rhs.digits_, digits_);
      sign_ = rsign;
    }
  }
  trim();
  return *this;
}

int2048 operator-(int2048 a, const int2048 &b) { return a -= b; }

// ===========================================================================
// Multiplication
// ===========================================================================

int2048 &int2048::operator*=(const int2048 &rhs) {
  digits_ = mul_mag(digits_, rhs.digits_);
  sign_ = sign_ * rhs.sign_;
  trim();
  return *this;
}

int2048 operator*(int2048 a, const int2048 &b) { return a *= b; }

// ===========================================================================
// Division and modulo
// ===========================================================================

int2048 &int2048::operator/=(const int2048 &rhs) {
  int2048 q, r;
  divmod_floor(*this, rhs, q, r);
  *this = std::move(q);
  return *this;
}

int2048 operator/(int2048 a, const int2048 &b) { return a /= b; }

int2048 &int2048::operator%=(const int2048 &rhs) {
  int2048 q, r;
  divmod_floor(*this, rhs, q, r);
  *this = std::move(r);
  return *this;
}

int2048 operator%(int2048 a, const int2048 &b) { return a %= b; }

// ===========================================================================
// Stream operators
// ===========================================================================

std::istream &operator>>(std::istream &is, int2048 &x) {
  std::string s;
  is >> s;
  x.read(s);
  return is;
}

std::ostream &operator<<(std::ostream &os, const int2048 &x) {
  if (x.is_zero()) {
    os << '0';
    return os;
  }
  std::string out;
  if (x.sign_ < 0)
    out.push_back('-');
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%d", x.digits_.back());
  out += buf;
  for (int i = static_cast<int>(x.digits_.size()) - 2; i >= 0; --i) {
    std::snprintf(buf, sizeof(buf), "%03d", x.digits_[i]);
    out += buf;
  }
  os << out;
  return os;
}

// ===========================================================================
// Comparison operators
// ===========================================================================

bool operator==(const int2048 &a, const int2048 &b) {
  return a.sign_ == b.sign_ && a.digits_ == b.digits_;
}

bool operator!=(const int2048 &a, const int2048 &b) { return !(a == b); }

bool operator<(const int2048 &a, const int2048 &b) {
  if (a.sign_ != b.sign_)
    return a.sign_ < b.sign_;
  int c = int2048::cmp_mag(a.digits_, b.digits_);
  return a.sign_ > 0 ? c < 0 : c > 0;
}

bool operator>(const int2048 &a, const int2048 &b) { return b < a; }
bool operator<=(const int2048 &a, const int2048 &b) { return !(b < a); }
bool operator>=(const int2048 &a, const int2048 &b) { return !(a < b); }

} // namespace sjtu
