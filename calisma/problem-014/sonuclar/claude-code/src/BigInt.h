#pragma once
#ifndef PYTHON_INTERPRETER_BIGINT_H
#define PYTHON_INTERPRETER_BIGINT_H

#include <cctype>
#include <climits>
#include <cstdint>
#include <string>
#include <vector>

// Arbitrary precision signed integer with a small-value fast path.
//
//  * Small mode (big_ is empty): the value is held directly in `s_` (any value
//    that fits in a signed 64-bit integer). No heap allocation, and the common
//    +, -, *, //, %, and comparison operations run as plain int64 arithmetic.
//  * Big mode (big_ non-empty): the value is `sign_ * magnitude(big_)` where the
//    magnitude is little-endian base 1e9 with no leading zeros. Only reached
//    when a value grows beyond the int64 range.
//
// Results are normalized so that anything fitting in int64 stays in small mode.
class BigInt {
public:
    static const long long BASE = 1000000000LL;
    static const int BASE_DIGITS = 9;

    long long s_ = 0;             // small-mode value (valid when big_ is empty)
    int sign_ = 1;                // big-mode sign (+1 / -1)
    std::vector<long long> big_;  // big-mode magnitude; empty => small mode

    BigInt() = default;
    BigInt(long long v) : s_(v) {}

    bool small() const { return big_.empty(); }
    bool isZero() const { return small() && s_ == 0; }  // big mode is never zero

    // ---- magnitude helpers (little-endian base 1e9 vectors) ----------------
    static std::vector<long long> magFromU(unsigned long long u) {
        std::vector<long long> m;
        while (u) {
            m.push_back(static_cast<long long>(u % static_cast<unsigned long long>(BASE)));
            u /= static_cast<unsigned long long>(BASE);
        }
        return m;
    }

    // Provide a (sign, magnitude) view for either representation mode.
    void bigView(int &sign, std::vector<long long> &mag) const {
        if (small()) {
            if (s_ < 0) {
                sign = -1;
                mag = magFromU(0ULL - static_cast<unsigned long long>(s_));
            } else {
                sign = 1;
                mag = magFromU(static_cast<unsigned long long>(s_));
            }
        } else {
            sign = sign_;
            mag = big_;
        }
    }

    // Build a normalized BigInt from a sign and a magnitude vector, collapsing
    // to small mode whenever the value fits in int64.
    static BigInt fromSignMag(int sign, std::vector<long long> mag) {
        while (!mag.empty() && mag.back() == 0) mag.pop_back();
        BigInt r;
        if (mag.empty()) return r;  // zero (small)
        if (mag.size() <= 2) {
            long long v = (mag.size() == 2) ? mag[1] * BASE + mag[0] : mag[0];
            r.s_ = (sign < 0) ? -v : v;
            return r;
        }
        r.sign_ = sign;
        r.big_ = std::move(mag);
        return r;
    }

    static int cmpMag(const std::vector<long long> &a, const std::vector<long long> &b) {
        if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
        for (int i = static_cast<int>(a.size()) - 1; i >= 0; i--)
            if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        return 0;
    }

    static std::vector<long long> addMag(const std::vector<long long> &a,
                                         const std::vector<long long> &b) {
        std::vector<long long> res;
        long long carry = 0;
        size_t n = std::max(a.size(), b.size());
        res.reserve(n + 1);
        for (size_t i = 0; i < n || carry; i++) {
            long long cur = carry;
            if (i < a.size()) cur += a[i];
            if (i < b.size()) cur += b[i];
            res.push_back(cur % BASE);
            carry = cur / BASE;
        }
        return res;
    }

    // requires magnitude(a) >= magnitude(b)
    static std::vector<long long> subMag(const std::vector<long long> &a,
                                         const std::vector<long long> &b) {
        std::vector<long long> res;
        long long borrow = 0;
        res.reserve(a.size());
        for (size_t i = 0; i < a.size(); i++) {
            long long cur = a[i] - borrow - (i < b.size() ? b[i] : 0);
            if (cur < 0) {
                cur += BASE;
                borrow = 1;
            } else {
                borrow = 0;
            }
            res.push_back(cur);
        }
        while (!res.empty() && res.back() == 0) res.pop_back();
        return res;
    }

    static std::vector<long long> mulMag(const std::vector<long long> &a,
                                         const std::vector<long long> &b) {
        if (a.empty() || b.empty()) return {};
        // a[i]*b[j] < 1e18; adding an existing limb (<1e9) and a carry (<1e9)
        // stays within unsigned long long range.
        std::vector<unsigned long long> res(a.size() + b.size(), 0);
        const unsigned long long base = static_cast<unsigned long long>(BASE);
        for (size_t i = 0; i < a.size(); i++) {
            unsigned long long carry = 0;
            for (size_t j = 0; j < b.size(); j++) {
                unsigned long long cur =
                    res[i + j] + static_cast<unsigned long long>(a[i]) * b[j] + carry;
                res[i + j] = cur % base;
                carry = cur / base;
            }
            size_t k = i + b.size();
            while (carry) {
                unsigned long long cur = res[k] + carry;
                res[k] = cur % base;
                carry = cur / base;
                k++;
            }
        }
        std::vector<long long> out(res.begin(), res.end());
        while (!out.empty() && out.back() == 0) out.pop_back();
        return out;
    }

    static std::vector<long long> mulSmall(const std::vector<long long> &a, long long x) {
        if (x == 0 || a.empty()) return {};
        std::vector<long long> res;
        res.reserve(a.size() + 1);
        const unsigned long long base = static_cast<unsigned long long>(BASE);
        unsigned long long carry = 0;
        for (size_t i = 0; i < a.size(); i++) {
            unsigned long long cur =
                static_cast<unsigned long long>(a[i]) * static_cast<unsigned long long>(x) + carry;
            res.push_back(static_cast<long long>(cur % base));
            carry = cur / base;
        }
        while (carry) {
            res.push_back(static_cast<long long>(carry % base));
            carry /= base;
        }
        while (!res.empty() && res.back() == 0) res.pop_back();
        return res;
    }

    // Unsigned magnitude division: A = Q*B + R with 0 <= R < B (B != 0).
    static void divmodMag(const std::vector<long long> &A, const std::vector<long long> &B,
                          std::vector<long long> &Q, std::vector<long long> &R) {
        Q.assign(A.size(), 0);
        R.clear();
        for (int i = static_cast<int>(A.size()) - 1; i >= 0; i--) {
            R.insert(R.begin(), A[i]);  // R = R * BASE + A[i]
            while (!R.empty() && R.back() == 0) R.pop_back();
            long long lo = 0, hi = BASE - 1, q = 0;
            while (lo <= hi) {
                long long mid = (lo + hi) / 2;
                if (cmpMag(mulSmall(B, mid), R) <= 0) {
                    q = mid;
                    lo = mid + 1;
                } else {
                    hi = mid - 1;
                }
            }
            Q[i] = q;
            if (q) R = subMag(R, mulSmall(B, q));
        }
        while (!Q.empty() && Q.back() == 0) Q.pop_back();
    }

    // ---- construction / rendering -----------------------------------------
    static BigInt fromString(const std::string &str) {
        size_t i = 0, n = str.size();
        while (i < n && std::isspace(static_cast<unsigned char>(str[i]))) i++;
        int sgn = 1;
        if (i < n && (str[i] == '+' || str[i] == '-')) {
            if (str[i] == '-') sgn = -1;
            i++;
        }
        size_t end = n;
        while (end > i && std::isspace(static_cast<unsigned char>(str[end - 1]))) end--;
        std::vector<long long> mag;
        for (int p = static_cast<int>(end); p > static_cast<int>(i); p -= BASE_DIGITS) {
            int start = p - BASE_DIGITS;
            if (start < static_cast<int>(i)) start = static_cast<int>(i);
            mag.push_back(std::stoll(str.substr(start, p - start)));
        }
        return fromSignMag(sgn, std::move(mag));
    }

    std::string toString() const {
        if (small()) return std::to_string(s_);
        std::string s;
        if (sign_ < 0) s += '-';
        s += std::to_string(big_.back());
        for (int i = static_cast<int>(big_.size()) - 2; i >= 0; i--) {
            std::string part = std::to_string(big_[i]);
            s += std::string(BASE_DIGITS - part.size(), '0');
            s += part;
        }
        return s;
    }

    double toDouble() const {
        if (small()) return static_cast<double>(s_);
        double r = 0;
        for (int i = static_cast<int>(big_.size()) - 1; i >= 0; i--)
            r = r * static_cast<double>(BASE) + static_cast<double>(big_[i]);
        return sign_ < 0 ? -r : r;
    }

    // Truncating conversion to long long (used for string-repeat counts, etc.).
    long long toLL() const {
        if (small()) return s_;
        long long r = 0;
        for (int i = static_cast<int>(big_.size()) - 1; i >= 0; i--) r = r * BASE + big_[i];
        return sign_ < 0 ? -r : r;
    }

    // ---- arithmetic --------------------------------------------------------
    BigInt operator-() const {
        if (small() && s_ != LLONG_MIN) {
            BigInt r;
            r.s_ = -s_;
            return r;
        }
        int sa;
        std::vector<long long> ma;
        bigView(sa, ma);
        return fromSignMag(-sa, std::move(ma));
    }

    BigInt operator+(const BigInt &o) const {
        if (small() && o.small()) {
            long long r;
            if (!__builtin_add_overflow(s_, o.s_, &r)) return BigInt(r);
        }
        int sa, sb;
        std::vector<long long> ma, mb;
        bigView(sa, ma);
        o.bigView(sb, mb);
        return addSigned(sa, ma, sb, mb);
    }

    BigInt operator-(const BigInt &o) const {
        if (small() && o.small()) {
            long long r;
            if (!__builtin_sub_overflow(s_, o.s_, &r)) return BigInt(r);
        }
        int sa, sb;
        std::vector<long long> ma, mb;
        bigView(sa, ma);
        o.bigView(sb, mb);
        return addSigned(sa, ma, -sb, mb);
    }

    BigInt operator*(const BigInt &o) const {
        if (small() && o.small()) {
            long long r;
            if (!__builtin_mul_overflow(s_, o.s_, &r)) return BigInt(r);
        }
        int sa, sb;
        std::vector<long long> ma, mb;
        bigView(sa, ma);
        o.bigView(sb, mb);
        return fromSignMag((sa == sb) ? 1 : -1, mulMag(ma, mb));
    }

    // Floor division (Python semantics: rounds toward negative infinity).
    BigInt floordiv(const BigInt &o) const {
        if (small() && o.small() && o.s_ != 0 && !(s_ == LLONG_MIN && o.s_ == -1)) {
            long long a = s_, b = o.s_, q = a / b, r = a % b;
            if (r != 0 && ((r < 0) != (b < 0))) q--;
            return BigInt(q);
        }
        int sa, sb;
        std::vector<long long> ma, mb;
        bigView(sa, ma);
        o.bigView(sb, mb);
        std::vector<long long> Q, R;
        divmodMag(ma, mb, Q, R);
        BigInt q = fromSignMag((sa == sb) ? 1 : -1, std::move(Q));
        if (!R.empty() && sa != sb) q = q - BigInt(1);  // adjust toward -inf
        return q;
    }

    // Python modulo: result has the sign of the divisor (a % b = a - (a//b)*b).
    BigInt mod(const BigInt &o) const {
        if (small() && o.small() && o.s_ != 0 && !(s_ == LLONG_MIN && o.s_ == -1)) {
            long long a = s_, b = o.s_, r = a % b;
            if (r != 0 && ((r < 0) != (b < 0))) r += b;
            return BigInt(r);
        }
        int sa, sb;
        std::vector<long long> ma, mb;
        bigView(sa, ma);
        o.bigView(sb, mb);
        std::vector<long long> Q, R;
        divmodMag(ma, mb, Q, R);
        if (R.empty()) return BigInt();
        if (sa == sb) return fromSignMag(sa, std::move(R));
        return fromSignMag(sb, subMag(mb, R));  // r = (|b| - R) with sign of b
    }

    int cmp(const BigInt &o) const {
        if (small() && o.small()) return s_ < o.s_ ? -1 : (s_ > o.s_ ? 1 : 0);
        int sa, sb;
        std::vector<long long> ma, mb;
        bigView(sa, ma);
        o.bigView(sb, mb);
        if (sa != sb) return sa < sb ? -1 : 1;
        int c = cmpMag(ma, mb);
        return sa < 0 ? -c : c;
    }

    bool operator==(const BigInt &o) const { return cmp(o) == 0; }
    bool operator<(const BigInt &o) const { return cmp(o) < 0; }

private:
    static BigInt addSigned(int sa, const std::vector<long long> &ma, int sb,
                            const std::vector<long long> &mb) {
        if (sa == sb) return fromSignMag(sa, addMag(ma, mb));
        int c = cmpMag(ma, mb);
        if (c == 0) return BigInt();
        if (c > 0) return fromSignMag(sa, subMag(ma, mb));
        return fromSignMag(sb, subMag(mb, ma));
    }
};

#endif  // PYTHON_INTERPRETER_BIGINT_H
