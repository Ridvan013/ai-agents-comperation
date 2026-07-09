#include "BigInt.h"

BigInt::BigInt() : isNegative(false) {
    digits.push_back(0);
}

BigInt::BigInt(long long v) {
    isNegative = v < 0;
    if (v == 0) {
        digits.push_back(0);
        isNegative = false;
    } else {
        unsigned long long uv = isNegative ? -v : v;
        while (uv > 0) {
            digits.push_back(uv % BASE);
            uv /= BASE;
        }
    }
}

BigInt::BigInt(const std::string& s) {
    isNegative = false;
    if (s.empty()) {
        digits.push_back(0);
        return;
    }
    int start = 0;
    if (s[0] == '-') {
        isNegative = true;
        start = 1;
    } else if (s[0] == '+') {
        start = 1;
    }
    
    // ignore leading zeros
    while(start < s.length() && s[start] == '0') {
        start++;
    }
    if (start == s.length()) {
        digits.push_back(0);
        isNegative = false;
        return;
    }

    for (int i = s.length(); i > start; i -= DIGIT_LEN) {
        if (i < start + DIGIT_LEN) {
            digits.push_back(std::stoi(s.substr(start, i - start)));
        } else {
            digits.push_back(std::stoi(s.substr(i - DIGIT_LEN, DIGIT_LEN)));
        }
    }
    trim();
}

BigInt::BigInt(const BigInt& other) = default;
BigInt& BigInt::operator=(const BigInt& other) = default;

void BigInt::trim() {
    while (digits.size() > 1 && digits.back() == 0) {
        digits.pop_back();
    }
    if (digits.size() == 1 && digits[0] == 0) {
        isNegative = false;
    }
}

bool BigInt::isZero() const {
    return digits.size() == 1 && digits[0] == 0;
}

std::string BigInt::toString() const {
    if (isZero()) return "0";
    std::string res = isNegative ? "-" : "";
    res += std::to_string(digits.back());
    for (int i = (int)digits.size() - 2; i >= 0; i--) {
        std::string s = std::to_string(digits[i]);
        res += std::string(DIGIT_LEN - s.length(), '0') + s;
    }
    return res;
}

BigInt BigInt::operator-() const {
    BigInt res = *this;
    if (!res.isZero()) res.isNegative = !res.isNegative;
    return res;
}

BigInt BigInt::operator+() const {
    return *this;
}

int BigInt::absCompare(const BigInt& a, const BigInt& b) {
    if (a.digits.size() != b.digits.size()) {
        return a.digits.size() < b.digits.size() ? -1 : 1;
    }
    for (int i = (int)a.digits.size() - 1; i >= 0; i--) {
        if (a.digits[i] != b.digits[i]) {
            return a.digits[i] < b.digits[i] ? -1 : 1;
        }
    }
    return 0;
}

bool BigInt::operator==(const BigInt& other) const {
    if (isNegative != other.isNegative) return false;
    if (digits.size() != other.digits.size()) return false;
    for (size_t i = 0; i < digits.size(); i++) {
        if (digits[i] != other.digits[i]) return false;
    }
    return true;
}

bool BigInt::operator!=(const BigInt& other) const {
    return !(*this == other);
}

bool BigInt::operator<(const BigInt& other) const {
    if (isNegative != other.isNegative) return isNegative;
    int cmp = absCompare(*this, other);
    if (isNegative) return cmp > 0;
    return cmp < 0;
}

bool BigInt::operator<=(const BigInt& other) const {
    return *this < other || *this == other;
}

bool BigInt::operator>(const BigInt& other) const {
    return !(*this <= other);
}

bool BigInt::operator>=(const BigInt& other) const {
    return !(*this < other);
}

BigInt BigInt::absAdd(const BigInt& a, const BigInt& b) {
    BigInt res;
    res.digits.clear();
    int carry = 0;
    for (size_t i = 0; i < std::max(a.digits.size(), b.digits.size()) || carry; i++) {
        if (i < a.digits.size()) carry += a.digits[i];
        if (i < b.digits.size()) carry += b.digits[i];
        res.digits.push_back(carry % BASE);
        carry /= BASE;
    }
    return res;
}

BigInt BigInt::absSub(const BigInt& a, const BigInt& b) {
    BigInt res;
    res.digits.clear();
    int carry = 0;
    for (size_t i = 0; i < a.digits.size(); i++) {
        carry += a.digits[i];
        if (i < b.digits.size()) carry -= b.digits[i];
        if (carry < 0) {
            res.digits.push_back(carry + BASE);
            carry = -1;
        } else {
            res.digits.push_back(carry);
            carry = 0;
        }
    }
    res.trim();
    return res;
}

BigInt BigInt::operator+(const BigInt& other) const {
    if (isNegative == other.isNegative) {
        BigInt res = absAdd(*this, other);
        res.isNegative = isNegative;
        return res;
    }
    if (absCompare(*this, other) >= 0) {
        BigInt res = absSub(*this, other);
        res.isNegative = isNegative;
        return res;
    }
    BigInt res = absSub(other, *this);
    res.isNegative = other.isNegative;
    return res;
}

BigInt BigInt::operator-(const BigInt& other) const {
    return *this + (-other);
}

BigInt BigInt::operator*(const BigInt& other) const {
    BigInt res;
    res.digits.assign(digits.size() + other.digits.size(), 0);
    for (size_t i = 0; i < digits.size(); i++) {
        long long carry = 0;
        for (size_t j = 0; j < other.digits.size() || carry; j++) {
            long long cur = res.digits[i + j] + digits[i] * 1LL * (j < other.digits.size() ? other.digits[j] : 0) + carry;
            res.digits[i + j] = cur % BASE;
            carry = cur / BASE;
        }
    }
    res.isNegative = isNegative != other.isNegative;
    res.trim();
    return res;
}

void BigInt::divMod(const BigInt& num, const BigInt& den, BigInt& q, BigInt& r) {
    if (den.isZero()) {
        throw std::runtime_error("ZeroDivisionError: integer division or modulo by zero");
    }
    BigInt absNum = num; absNum.isNegative = false;
    BigInt absDen = den; absDen.isNegative = false;

    if (absCompare(absNum, absDen) < 0) {
        q = BigInt(0);
        r = absNum;
    } else {
        q.digits.assign(absNum.digits.size(), 0);
        r = BigInt(0);
        for (int i = (int)absNum.digits.size() - 1; i >= 0; i--) {
            r = r * BigInt(BASE) + BigInt(absNum.digits[i]);
            long long left = 0, right = BASE - 1, ans = 0;
            while (left <= right) {
                long long mid = left + (right - left) / 2;
                BigInt t = absDen * BigInt(mid);
                if (t <= r) {
                    ans = mid;
                    left = mid + 1;
                } else {
                    right = mid - 1;
                }
            }
            q.digits[i] = ans;
            r = r - absDen * BigInt(ans);
        }
        q.trim();
        r.trim();
    }
    
    // Python semantics: floor division
    if (num.isNegative != den.isNegative) {
        if (!r.isZero()) {
            q = q + BigInt(1);
            r = absDen - r;
        }
        q.isNegative = !q.isZero();
    }
    // r sign matches denominator in Python
    if (!r.isZero()) {
        r.isNegative = den.isNegative;
    }
}

BigInt BigInt::operator/(const BigInt& other) const {
    BigInt q, r;
    divMod(*this, other, q, r);
    return q;
}

BigInt BigInt::operator%(const BigInt& other) const {
    BigInt q, r;
    divMod(*this, other, q, r);
    return r;
}

BigInt& BigInt::operator+=(const BigInt& other) { return *this = *this + other; }
BigInt& BigInt::operator-=(const BigInt& other) { return *this = *this - other; }
BigInt& BigInt::operator*=(const BigInt& other) { return *this = *this * other; }
BigInt& BigInt::operator/=(const BigInt& other) { return *this = *this / other; }
BigInt& BigInt::operator%=(const BigInt& other) { return *this = *this % other; }
