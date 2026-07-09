#ifndef VALUE_H
#define VALUE_H

#include "BigInt.h"
#include <string>
#include <variant>
#include <stdexcept>
#include <iostream>

struct NoneType {
    bool operator==(const NoneType&) const { return true; }
    bool operator!=(const NoneType&) const { return false; }
};

class Value {
public:
    enum Type {
        NONE, BOOL, INT, FLOAT, STRING, TUPLE
    };

    std::variant<NoneType, bool, BigInt, double, std::string> data;

    Value();
    Value(NoneType n);
    Value(bool b);
    Value(const BigInt& i);
    Value(long long i);
    Value(double d);
    Value(const std::string& s);
    Value(const char* s);

    Type type() const;

    bool isNone() const;
    bool isBool() const;
    bool isInt() const;
    bool isFloat() const;
    bool isString() const;

    bool toBool() const;
    BigInt toBigInt() const;
    double toDouble() const;
    std::string toString() const;
    std::string toRepr() const;

    // Unary operators
    Value operator-() const;
    Value operator+() const;
    Value operator!() const;

    // Relational operators
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
    bool operator<(const Value& other) const;
    bool operator<=(const Value& other) const;
    bool operator>(const Value& other) const;
    bool operator>=(const Value& other) const;

    // Arithmetic operators
    Value operator+(const Value& other) const;
    Value operator-(const Value& other) const;
    Value operator*(const Value& other) const;
    Value operator/(const Value& other) const;
    Value operator%(const Value& other) const;
    Value intDiv(const Value& other) const; // For // operator

    // Augmented Assignment
    Value& operator+=(const Value& other);
    Value& operator-=(const Value& other);
    Value& operator*=(const Value& other);
    Value& operator/=(const Value& other);
    Value& operator%=(const Value& other);
};

#endif // VALUE_H
