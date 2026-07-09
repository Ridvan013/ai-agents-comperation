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
  static const int BASE = 10000;
  static const int BASE_DIGITS = 4;

  std::vector<int> digits;
  bool negative;

  void trim();
  bool is_zero() const;
  int compare_abs(const int2048 &) const;
  void add_abs(const int2048 &);
  void sub_abs(const int2048 &);

  static void fft(std::vector<std::complex<long double>> &, bool);
  static int2048 multiply_naive(const int2048 &, const int2048 &);
  static int2048 multiply_fft(const int2048 &, const int2048 &);
  static int2048 multiply_abs(const int2048 &, const int2048 &);
  static int2048 multiply_by_int(const int2048 &, int);
  static void divmod_abs(const int2048 &, const int2048 &, int2048 &, int2048 &);

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


namespace {
long double get_pi() {
  return std::acos(static_cast<long double>(-1.0));
}
} // namespace

namespace sjtu {

int2048::int2048() : negative(false) {}

int2048::int2048(long long value) : negative(false) {
  unsigned long long magnitude;
  if (value < 0) {
    negative = true;
    magnitude = static_cast<unsigned long long>(-(value + 1)) + 1;
  } else {
    magnitude = static_cast<unsigned long long>(value);
  }
  while (magnitude > 0) {
    digits.push_back(static_cast<int>(magnitude % BASE));
    magnitude /= BASE;
  }
  trim();
}

int2048::int2048(const std::string &value) : negative(false) {
  read(value);
}

int2048::int2048(const int2048 &other)
    : digits(other.digits), negative(other.negative) {}

void int2048::trim() {
  while (!digits.empty() && digits.back() == 0) {
    digits.pop_back();
  }
  if (digits.empty()) {
    negative = false;
  }
}

bool int2048::is_zero() const {
  return digits.empty();
}

int int2048::compare_abs(const int2048 &other) const {
  if (digits.size() != other.digits.size()) {
    return digits.size() < other.digits.size() ? -1 : 1;
  }
  for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
    if (digits[i] != other.digits[i]) {
      return digits[i] < other.digits[i] ? -1 : 1;
    }
  }
  return 0;
}

void int2048::add_abs(const int2048 &other) {
  if (digits.size() < other.digits.size()) {
    digits.resize(other.digits.size(), 0);
  }
  int carry = 0;
  for (std::size_t i = 0; i < digits.size(); ++i) {
    int sum = digits[i] + carry;
    if (i < other.digits.size()) {
      sum += other.digits[i];
    }
    if (sum >= BASE) {
      sum -= BASE;
      carry = 1;
    } else {
      carry = 0;
    }
    digits[i] = sum;
  }
  if (carry != 0) {
    digits.push_back(carry);
  }
}

void int2048::sub_abs(const int2048 &other) {
  int borrow = 0;
  for (std::size_t i = 0; i < digits.size(); ++i) {
    int diff = digits[i] - borrow;
    if (i < other.digits.size()) {
      diff -= other.digits[i];
    }
    if (diff < 0) {
      diff += BASE;
      borrow = 1;
    } else {
      borrow = 0;
    }
    digits[i] = diff;
  }
  trim();
}

void int2048::fft(std::vector<std::complex<long double>> &values, bool invert) {
  int n = static_cast<int>(values.size());
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    while ((j & bit) != 0) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;
    if (i < j) {
      std::complex<long double> temp = values[i];
      values[i] = values[j];
      values[j] = temp;
    }
  }

  const long double pi = get_pi();
  for (int len = 2; len <= n; len <<= 1) {
    long double angle = 2.0L * pi / static_cast<long double>(len);
    if (invert) {
      angle = -angle;
    }
    std::complex<long double> wlen(std::cos(angle), std::sin(angle));
    for (int start = 0; start < n; start += len) {
      std::complex<long double> w(1.0L, 0.0L);
      int half = len >> 1;
      for (int offset = 0; offset < half; ++offset) {
        std::complex<long double> u = values[start + offset];
        std::complex<long double> v = values[start + offset + half] * w;
        values[start + offset] = u + v;
        values[start + offset + half] = u - v;
        w *= wlen;
      }
    }
  }

  if (invert) {
    for (int i = 0; i < n; ++i) {
      values[i] /= static_cast<long double>(n);
    }
  }
}

int2048 int2048::multiply_naive(const int2048 &lhs, const int2048 &rhs) {
  int2048 result;
  if (lhs.is_zero() || rhs.is_zero()) {
    return result;
  }
  result.digits.assign(lhs.digits.size() + rhs.digits.size(), 0);
  for (std::size_t i = 0; i < lhs.digits.size(); ++i) {
    long long carry = 0;
    for (std::size_t j = 0; j < rhs.digits.size() || carry != 0; ++j) {
      long long current = result.digits[i + j] + carry;
      if (j < rhs.digits.size()) {
        current += static_cast<long long>(lhs.digits[i]) * rhs.digits[j];
      }
      result.digits[i + j] = static_cast<int>(current % BASE);
      carry = current / BASE;
    }
  }
  result.trim();
  return result;
}

int2048 int2048::multiply_fft(const int2048 &lhs, const int2048 &rhs) {
  int2048 result;
  if (lhs.is_zero() || rhs.is_zero()) {
    return result;
  }

  int n = 1;
  int target =
      static_cast<int>(lhs.digits.size() + rhs.digits.size());
  while (n < target) {
    n <<= 1;
  }

  std::vector<std::complex<long double>> left(n);
  std::vector<std::complex<long double>> right(n);
  for (std::size_t i = 0; i < lhs.digits.size(); ++i) {
    left[i] = std::complex<long double>(lhs.digits[i], 0.0L);
  }
  for (std::size_t i = 0; i < rhs.digits.size(); ++i) {
    right[i] = std::complex<long double>(rhs.digits[i], 0.0L);
  }

  fft(left, false);
  fft(right, false);
  for (int i = 0; i < n; ++i) {
    left[i] *= right[i];
  }
  fft(left, true);

  result.digits.assign(target, 0);
  long long carry = 0;
  for (int i = 0; i < target; ++i) {
    long double real_part = left[i].real();
    long long coefficient = static_cast<long long>(
        real_part + (real_part >= 0.0L ? 0.5L : -0.5L));
    long long current = coefficient + carry;
    result.digits[i] = static_cast<int>(current % BASE);
    carry = current / BASE;
  }
  while (carry != 0) {
    result.digits.push_back(static_cast<int>(carry % BASE));
    carry /= BASE;
  }
  result.trim();
  return result;
}

int2048 int2048::multiply_abs(const int2048 &lhs, const int2048 &rhs) {
  const std::size_t threshold = 64;
  if (lhs.digits.size() < threshold || rhs.digits.size() < threshold) {
    return multiply_naive(lhs, rhs);
  }
  return multiply_fft(lhs, rhs);
}

int2048 int2048::multiply_by_int(const int2048 &value, int factor) {
  int2048 result;
  if (factor == 0 || value.is_zero()) {
    return result;
  }
  result.digits.assign(value.digits.size(), 0);
  long long carry = 0;
  for (std::size_t i = 0; i < value.digits.size(); ++i) {
    long long current = static_cast<long long>(value.digits[i]) * factor + carry;
    result.digits[i] = static_cast<int>(current % BASE);
    carry = current / BASE;
  }
  while (carry != 0) {
    result.digits.push_back(static_cast<int>(carry % BASE));
    carry /= BASE;
  }
  result.trim();
  return result;
}

void int2048::divmod_abs(const int2048 &lhs, const int2048 &rhs, int2048 &quotient,
                         int2048 &remainder) {
  quotient = int2048();
  remainder = int2048();
  if (lhs.compare_abs(rhs) < 0) {
    remainder = lhs;
    return;
  }

  int norm = BASE / (rhs.digits.back() + 1);
  int2048 dividend = multiply_by_int(lhs, norm);
  int2048 divisor = multiply_by_int(rhs, norm);

  quotient.digits.assign(dividend.digits.size(), 0);
  for (int i = static_cast<int>(dividend.digits.size()) - 1; i >= 0; --i) {
    remainder.digits.insert(remainder.digits.begin(), dividend.digits[i]);
    remainder.trim();

    int divisor_size = static_cast<int>(divisor.digits.size());
    int remainder_size = static_cast<int>(remainder.digits.size());
    long long top1 = remainder_size > divisor_size ? remainder.digits[divisor_size] : 0;
    long long top2 =
        remainder_size > divisor_size - 1 ? remainder.digits[divisor_size - 1] : 0;
    long long estimate = (top1 * BASE + top2) / divisor.digits.back();
    if (estimate >= BASE) {
      estimate = BASE - 1;
    }

    int2048 candidate = multiply_by_int(divisor, static_cast<int>(estimate));
    while (remainder.compare_abs(candidate) < 0) {
      --estimate;
      candidate.sub_abs(divisor);
    }
    remainder.sub_abs(candidate);
    quotient.digits[i] = static_cast<int>(estimate);
  }

  quotient.trim();
  long long carry = 0;
  for (int i = static_cast<int>(remainder.digits.size()) - 1; i >= 0; --i) {
    long long current = remainder.digits[i] + carry * BASE;
    remainder.digits[i] = static_cast<int>(current / norm);
    carry = current % norm;
  }
  remainder.trim();
}

void int2048::read(const std::string &value) {
  digits.clear();
  negative = false;

  std::size_t pos = 0;
  if (!value.empty() && (value[0] == '-' || value[0] == '+')) {
    negative = value[0] == '-';
    pos = 1;
  }
  while (pos < value.size() && value[pos] == '0') {
    ++pos;
  }
  if (pos == value.size()) {
    negative = false;
    return;
  }

  for (std::size_t end = value.size(); end > pos;) {
    std::size_t begin = end >= static_cast<std::size_t>(BASE_DIGITS)
                            ? end - BASE_DIGITS
                            : 0;
    if (begin < pos) {
      begin = pos;
    }
    int block = 0;
    for (std::size_t i = begin; i < end; ++i) {
      block = block * 10 + (value[i] - '0');
    }
    digits.push_back(block);
    end = begin;
  }
  trim();
}

void int2048::print() {
  std::cout << *this;
}

int2048 &int2048::add(const int2048 &other) {
  return *this += other;
}

int2048 add(int2048 lhs, const int2048 &rhs) {
  return lhs += rhs;
}

int2048 &int2048::minus(const int2048 &other) {
  return *this -= other;
}

int2048 minus(int2048 lhs, const int2048 &rhs) {
  return lhs -= rhs;
}

int2048 int2048::operator+() const {
  return *this;
}

int2048 int2048::operator-() const {
  int2048 result(*this);
  if (!result.is_zero()) {
    result.negative = !result.negative;
  }
  return result;
}

int2048 &int2048::operator=(const int2048 &other) {
  if (this != &other) {
    digits = other.digits;
    negative = other.negative;
  }
  return *this;
}

int2048 &int2048::operator+=(const int2048 &other) {
  if (negative == other.negative) {
    add_abs(other);
    return *this;
  }

  int cmp = compare_abs(other);
  if (cmp >= 0) {
    sub_abs(other);
  } else {
    int2048 result(other);
    result.sub_abs(*this);
    *this = result;
  }
  return *this;
}

int2048 operator+(int2048 lhs, const int2048 &rhs) {
  return lhs += rhs;
}

int2048 &int2048::operator-=(const int2048 &other) {
  return *this += (-other);
}

int2048 operator-(int2048 lhs, const int2048 &rhs) {
  return lhs -= rhs;
}

int2048 &int2048::operator*=(const int2048 &other) {
  bool result_negative = negative != other.negative;
  int2048 lhs(*this);
  int2048 rhs(other);
  lhs.negative = false;
  rhs.negative = false;
  *this = multiply_abs(lhs, rhs);
  if (!is_zero()) {
    negative = result_negative;
  }
  return *this;
}

int2048 operator*(int2048 lhs, const int2048 &rhs) {
  return lhs *= rhs;
}

int2048 &int2048::operator/=(const int2048 &other) {
  int2048 lhs(*this);
  int2048 rhs(other);
  lhs.negative = false;
  rhs.negative = false;

  int2048 quotient;
  int2048 remainder;
  divmod_abs(lhs, rhs, quotient, remainder);

  if (negative == other.negative) {
    *this = quotient;
    if (!remainder.is_zero() && other.negative) {
      remainder.negative = true;
    }
    return *this;
  }

  if (remainder.is_zero()) {
    *this = quotient;
    if (!is_zero()) {
      negative = true;
    }
    return *this;
  }

  quotient += int2048(1);
  quotient.negative = true;
  *this = quotient;
  return *this;
}

int2048 operator/(int2048 lhs, const int2048 &rhs) {
  return lhs /= rhs;
}

int2048 &int2048::operator%=(const int2048 &other) {
  int2048 lhs(*this);
  int2048 rhs(other);
  lhs.negative = false;
  rhs.negative = false;

  int2048 quotient;
  int2048 remainder;
  divmod_abs(lhs, rhs, quotient, remainder);

  if (negative == other.negative) {
    *this = remainder;
    if (!is_zero() && other.negative) {
      negative = true;
    }
    return *this;
  }

  if (remainder.is_zero()) {
    *this = remainder;
    return *this;
  }

  *this = rhs - remainder;
  if (!is_zero() && other.negative) {
    negative = true;
  }
  return *this;
}

int2048 operator%(int2048 lhs, const int2048 &rhs) {
  return lhs %= rhs;
}

std::istream &operator>>(std::istream &is, int2048 &value) {
  std::string text;
  is >> text;
  value.read(text);
  return is;
}

std::ostream &operator<<(std::ostream &os, const int2048 &value) {
  if (value.is_zero()) {
    os << '0';
    return os;
  }
  if (value.negative) {
    os << '-';
  }
  os << value.digits.back();
  char buffer[8];
  for (int i = static_cast<int>(value.digits.size()) - 2; i >= 0; --i) {
    std::sprintf(buffer, "%04d", value.digits[i]);
    os << buffer;
  }
  return os;
}

bool operator==(const int2048 &lhs, const int2048 &rhs) {
  return lhs.negative == rhs.negative && lhs.digits == rhs.digits;
}

bool operator!=(const int2048 &lhs, const int2048 &rhs) {
  return !(lhs == rhs);
}

bool operator<(const int2048 &lhs, const int2048 &rhs) {
  if (lhs.negative != rhs.negative) {
    return lhs.negative;
  }
  int cmp = lhs.compare_abs(rhs);
  if (lhs.negative) {
    return cmp > 0;
  }
  return cmp < 0;
}

bool operator>(const int2048 &lhs, const int2048 &rhs) {
  return rhs < lhs;
}

bool operator<=(const int2048 &lhs, const int2048 &rhs) {
  return !(rhs < lhs);
}

bool operator>=(const int2048 &lhs, const int2048 &rhs) {
  return !(lhs < rhs);
}

} // namespace sjtu
