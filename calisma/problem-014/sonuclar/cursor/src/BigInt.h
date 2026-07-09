#pragma once
#ifndef PYTHON_INTERPRETER_BIGINT_H
#define PYTHON_INTERPRETER_BIGINT_H

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>

// Arbitrary precision signed integer.
// Magnitude is stored little-endian in base 1e9 limbs; the empty limb vector
// represents zero. `neg` is only meaningful for non-zero values.
class BigInt {
public:
	static constexpr uint32_t BASE = 1000000000u;
	static constexpr int BASE_DIGITS = 9;

	bool neg = false;
	std::vector<uint32_t> mag;

	BigInt() = default;

	BigInt(long long v) {
		if (v < 0) {
			neg = true;
			// careful with LLONG_MIN
			unsigned long long uv = (v == INT64_MIN)
				? (static_cast<unsigned long long>(INT64_MAX) + 1ull)
				: static_cast<unsigned long long>(-v);
			fromUnsigned(uv);
		} else {
			fromUnsigned(static_cast<unsigned long long>(v));
		}
	}

	explicit BigInt(const std::string &s) { parse(s); }

	// ---- basic queries -------------------------------------------------
	bool isZero() const { return mag.empty(); }

	int sign() const { return isZero() ? 0 : (neg ? -1 : 1); }

	// ---- unary ---------------------------------------------------------
	BigInt operator-() const {
		BigInt r = *this;
		if (!r.isZero()) r.neg = !r.neg;
		return r;
	}

	// ---- comparison ----------------------------------------------------
	// returns -1,0,1 for signed comparison of *this and o
	int compare(const BigInt &o) const {
		if (neg != o.neg) {
			if (isZero() && o.isZero()) return 0;
			return neg ? -1 : 1;
		}
		int c = cmpMag(mag, o.mag);
		return neg ? -c : c;
	}

	bool operator==(const BigInt &o) const { return compare(o) == 0; }
	bool operator!=(const BigInt &o) const { return compare(o) != 0; }
	bool operator<(const BigInt &o) const { return compare(o) < 0; }
	bool operator<=(const BigInt &o) const { return compare(o) <= 0; }
	bool operator>(const BigInt &o) const { return compare(o) > 0; }
	bool operator>=(const BigInt &o) const { return compare(o) >= 0; }

	// ---- arithmetic ----------------------------------------------------
	BigInt operator+(const BigInt &o) const {
		BigInt r;
		if (neg == o.neg) {
			r.mag = addMag(mag, o.mag);
			r.neg = neg;
		} else {
			int c = cmpMag(mag, o.mag);
			if (c == 0) return BigInt();
			if (c > 0) {
				r.mag = subMag(mag, o.mag);
				r.neg = neg;
			} else {
				r.mag = subMag(o.mag, mag);
				r.neg = o.neg;
			}
		}
		r.normalize();
		return r;
	}

	BigInt operator-(const BigInt &o) const { return *this + (-o); }

	BigInt operator*(const BigInt &o) const {
		BigInt r;
		r.mag = mulMag(mag, o.mag);
		r.neg = (neg != o.neg);
		r.normalize();
		return r;
	}

	// Python-style floor division and modulo.
	// Requires divisor != 0. Fills q = floor(a/b), r = a - q*b (sign of b).
	static void floorDivMod(const BigInt &a, const BigInt &b, BigInt &q, BigInt &r) {
		std::vector<uint32_t> Q, R;
		divmodMag(a.mag, b.mag, Q, R);

		bool sameSign = (a.neg == b.neg);
		if (R.empty() || sameSign) {
			q.mag = Q;
			q.neg = (a.neg != b.neg);
			q.normalize();
			r.mag = R;
			r.neg = a.neg; // remainder carries sign of a here (== sign of b when sameSign)
			r.normalize();
		} else {
			// opposite signs with non-zero remainder: adjust toward -inf
			q.mag = addMag(Q, std::vector<uint32_t>{1});
			q.neg = true;
			q.normalize();
			r.mag = subMag(b.mag, R); // |b| - R
			r.neg = b.neg;
			r.normalize();
		}
	}

	BigInt operator/(const BigInt &o) const { // floor division
		BigInt q, r;
		floorDivMod(*this, o, q, r);
		return q;
	}

	BigInt operator%(const BigInt &o) const {
		BigInt q, r;
		floorDivMod(*this, o, q, r);
		return r;
	}

	// ---- conversions ---------------------------------------------------
	double toDouble() const {
		double res = 0.0;
		for (auto it = mag.rbegin(); it != mag.rend(); ++it)
			res = res * static_cast<double>(BASE) + static_cast<double>(*it);
		return neg ? -res : res;
	}

	// Truncates toward zero into a long long (assumes it fits).
	long long toLongLong() const {
		long long res = 0;
		for (auto it = mag.rbegin(); it != mag.rend(); ++it)
			res = res * static_cast<long long>(BASE) + static_cast<long long>(*it);
		return neg ? -res : res;
	}

	std::string toString() const {
		if (mag.empty()) return "0";
		std::string s;
		if (neg) s.push_back('-');
		s += std::to_string(mag.back());
		char buf[16];
		for (int i = static_cast<int>(mag.size()) - 2; i >= 0; --i) {
			std::snprintf(buf, sizeof(buf), "%09u", mag[i]);
			s += buf;
		}
		return s;
	}

	// Builds a BigInt from a (possibly large) double, truncating toward zero.
	static BigInt fromDouble(double d) {
		BigInt r;
		bool negative = d < 0;
		d = std::trunc(negative ? -d : d);
		std::vector<uint32_t> limbs;
		const double B = static_cast<double>(BASE);
		while (d >= 1.0) {
			double q = std::floor(d / B);
			double rem = d - q * B;
			limbs.push_back(static_cast<uint32_t>(rem));
			d = q;
		}
		r.mag = std::move(limbs);
		r.neg = negative;
		r.normalize();
		return r;
	}

private:
	void fromUnsigned(unsigned long long v) {
		mag.clear();
		while (v) {
			mag.push_back(static_cast<uint32_t>(v % BASE));
			v /= BASE;
		}
	}

	void parse(const std::string &s) {
		mag.clear();
		neg = false;
		size_t i = 0;
		if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
			neg = (s[i] == '-');
			++i;
		}
		// skip leading zeros for a clean magnitude
		size_t start = i;
		std::string digits = s.substr(start);
		// strip surrounding whitespace-safe: assume caller trimmed
		int len = static_cast<int>(digits.size());
		for (int pos = len; pos > 0; pos -= BASE_DIGITS) {
			int from = std::max(0, pos - BASE_DIGITS);
			mag.push_back(static_cast<uint32_t>(std::stoul(digits.substr(from, pos - from))));
		}
		normalize();
	}

	void normalize() {
		while (!mag.empty() && mag.back() == 0) mag.pop_back();
		if (mag.empty()) neg = false;
	}

	// ---- magnitude helpers (operate on bare limb vectors) --------------
	static int cmpMag(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
		if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
		for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i)
			if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
		return 0;
	}

	static std::vector<uint32_t> addMag(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
		std::vector<uint32_t> r;
		r.reserve(std::max(a.size(), b.size()) + 1);
		uint64_t carry = 0;
		size_t n = std::max(a.size(), b.size());
		for (size_t i = 0; i < n; ++i) {
			uint64_t cur = carry;
			if (i < a.size()) cur += a[i];
			if (i < b.size()) cur += b[i];
			r.push_back(static_cast<uint32_t>(cur % BASE));
			carry = cur / BASE;
		}
		if (carry) r.push_back(static_cast<uint32_t>(carry));
		return r;
	}

	// requires a >= b (magnitude)
	static std::vector<uint32_t> subMag(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
		std::vector<uint32_t> r;
		r.reserve(a.size());
		int64_t borrow = 0;
		for (size_t i = 0; i < a.size(); ++i) {
			int64_t cur = static_cast<int64_t>(a[i]) - borrow - (i < b.size() ? b[i] : 0);
			if (cur < 0) {
				cur += BASE;
				borrow = 1;
			} else {
				borrow = 0;
			}
			r.push_back(static_cast<uint32_t>(cur));
		}
		while (!r.empty() && r.back() == 0) r.pop_back();
		return r;
	}

	static std::vector<uint32_t> mulMag(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
		if (a.empty() || b.empty()) return {};
		std::vector<uint64_t> acc(a.size() + b.size(), 0);
		for (size_t i = 0; i < a.size(); ++i) {
			uint64_t carry = 0;
			uint64_t ai = a[i];
			for (size_t j = 0; j < b.size(); ++j) {
				uint64_t cur = acc[i + j] + ai * b[j] + carry;
				acc[i + j] = cur % BASE;
				carry = cur / BASE;
			}
			size_t k = i + b.size();
			while (carry) {
				uint64_t cur = acc[k] + carry;
				acc[k] = cur % BASE;
				carry = cur / BASE;
				++k;
			}
		}
		std::vector<uint32_t> r(acc.size());
		for (size_t i = 0; i < acc.size(); ++i) r[i] = static_cast<uint32_t>(acc[i]);
		while (!r.empty() && r.back() == 0) r.pop_back();
		return r;
	}

	// multiply magnitude by a single value x in [0, BASE)
	static std::vector<uint32_t> mulSmall(const std::vector<uint32_t> &a, uint64_t x) {
		if (x == 0 || a.empty()) return {};
		std::vector<uint32_t> r;
		r.reserve(a.size() + 1);
		uint64_t carry = 0;
		for (size_t i = 0; i < a.size(); ++i) {
			uint64_t cur = a[i] * x + carry;
			r.push_back(static_cast<uint32_t>(cur % BASE));
			carry = cur / BASE;
		}
		while (carry) {
			r.push_back(static_cast<uint32_t>(carry % BASE));
			carry /= BASE;
		}
		return r;
	}

	// Any magnitude of at most 4 limbs is < 1e36 and therefore fits an
	// unsigned __int128, letting us divide small operands in O(1).
	static unsigned __int128 pack(const std::vector<uint32_t> &a) {
		unsigned __int128 v = 0;
		for (auto it = a.rbegin(); it != a.rend(); ++it)
			v = v * BASE + *it;
		return v;
	}
	static std::vector<uint32_t> unpack(unsigned __int128 v) {
		std::vector<uint32_t> r;
		while (v) {
			r.push_back(static_cast<uint32_t>(v % BASE));
			v /= BASE;
		}
		return r;
	}

	// unsigned long division: a = b*q + r, 0 <= r < b, b != 0
	static void divmodMag(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b,
						  std::vector<uint32_t> &q, std::vector<uint32_t> &r) {
		q.clear();
		r.clear();
		if (b.empty()) throw std::runtime_error("division by zero");
		if (cmpMag(a, b) < 0) {
			r = a;
			return;
		}
		if (a.size() <= 4 && b.size() <= 4) {
			unsigned __int128 av = pack(a), bv = pack(b);
			q = unpack(av / bv);
			r = unpack(av % bv);
			return;
		}
		q.assign(a.size(), 0);
		std::vector<uint32_t> cur; // running remainder magnitude
		for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
			// cur = cur * BASE + a[i]
			cur.insert(cur.begin(), a[i]);
			while (!cur.empty() && cur.back() == 0) cur.pop_back();
			// binary search largest x in [0, BASE) with b*x <= cur
			uint32_t lo = 0, hi = BASE - 1, x = 0;
			while (lo <= hi) {
				uint32_t mid = lo + (hi - lo) / 2;
				if (cmpMag(mulSmall(b, mid), cur) <= 0) {
					x = mid;
					lo = mid + 1;
				} else {
					if (mid == 0) break;
					hi = mid - 1;
				}
			}
			q[i] = x;
			cur = subMag(cur, mulSmall(b, x));
		}
		while (!q.empty() && q.back() == 0) q.pop_back();
		r = cur;
	}
};

#endif // PYTHON_INTERPRETER_BIGINT_H
