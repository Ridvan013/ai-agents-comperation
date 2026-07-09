#ifndef BIGINT_H
#define BIGINT_H

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>

class BigInt {
public:
    std::vector<int> digits; // Base 10^9
    bool isNegative;

    static const int BASE = 1e9;
    static const int DIGIT_LEN = 9;

    BigInt();
    BigInt(long long v);
    BigInt(const std::string& s);
    BigInt(const BigInt& other);

    BigInt& operator=(const BigInt& other);

    // Unary
    BigInt operator-() const;
    BigInt operator+() const;

    // Relational
    bool operator==(const BigInt& other) const;
    bool operator!=(const BigInt& other) const;
    bool operator<(const BigInt& other) const;
    bool operator<=(const BigInt& other) const;
    bool operator>(const BigInt& other) const;
    bool operator>=(const BigInt& other) const;

    // Arithmetic
    BigInt operator+(const BigInt& other) const;
    BigInt operator-(const BigInt& other) const;
    BigInt operator*(const BigInt& other) const;
    BigInt operator/(const BigInt& other) const;
    BigInt operator%(const BigInt& other) const;

    // Assignment
    BigInt& operator+=(const BigInt& other);
    BigInt& operator-=(const BigInt& other);
    BigInt& operator*=(const BigInt& other);
    BigInt& operator/=(const BigInt& other);
    BigInt& operator%=(const BigInt& other);

    // Utilities
    std::string toString() const;
    bool isZero() const;

    // Helpers
    static BigInt absAdd(const BigInt& a, const BigInt& b);
    static BigInt absSub(const BigInt& a, const BigInt& b); // assumes |a| >= |b|
    static int absCompare(const BigInt& a, const BigInt& b);
    static void divMod(const BigInt& num, const BigInt& den, BigInt& q, BigInt& r);

    void trim();
};

#endif // BIGINT_H
