#include "Value.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

Value::Value() : data(NoneType{}) {}
Value::Value(NoneType n) : data(n) {}
Value::Value(bool b) : data(b) {}
Value::Value(const BigInt& i) : data(i) {}
Value::Value(long long i) : data(BigInt(i)) {}
Value::Value(double d) : data(d) {}
Value::Value(const std::string& s) : data(s) {}
Value::Value(const char* s) : data(std::string(s)) {}

Value::Type Value::type() const {
    if (std::holds_alternative<NoneType>(data)) return NONE;
    if (std::holds_alternative<bool>(data)) return BOOL;
    if (std::holds_alternative<BigInt>(data)) return INT;
    if (std::holds_alternative<double>(data)) return FLOAT;
    if (std::holds_alternative<std::string>(data)) return STRING;
    return NONE;
}

bool Value::isNone() const { return type() == NONE; }
bool Value::isBool() const { return type() == BOOL; }
bool Value::isInt() const { return type() == INT; }
bool Value::isFloat() const { return type() == FLOAT; }
bool Value::isString() const { return type() == STRING; }

bool Value::toBool() const {
    if (isNone()) return false;
    if (isBool()) return std::get<bool>(data);
    if (isInt()) return !std::get<BigInt>(data).isZero();
    if (isFloat()) return std::get<double>(data) != 0.0;
    if (isString()) return !std::get<std::string>(data).empty();
    return false;
}

BigInt Value::toBigInt() const {
    if (isBool()) return BigInt(std::get<bool>(data) ? 1 : 0);
    if (isInt()) return std::get<BigInt>(data);
    if (isFloat()) return BigInt((long long)std::get<double>(data)); // Might lose precision, but typical for conversion
    if (isString()) return BigInt(std::get<std::string>(data));
    throw std::runtime_error("Cannot convert to int");
}

double Value::toDouble() const {
    if (isBool()) return std::get<bool>(data) ? 1.0 : 0.0;
    if (isInt()) return std::stod(std::get<BigInt>(data).toString());
    if (isFloat()) return std::get<double>(data);
    if (isString()) return std::stod(std::get<std::string>(data));
    throw std::runtime_error("Cannot convert to float");
}

std::string Value::toString() const {
    if (isNone()) return "None";
    if (isBool()) return std::get<bool>(data) ? "True" : "False";
    if (isInt()) return std::get<BigInt>(data).toString();
    if (isFloat()) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(6) << std::get<double>(data);
        return out.str();
    }
    if (isString()) return std::get<std::string>(data);
    return "";
}

Value Value::operator-() const {
    if (isInt()) return Value(-std::get<BigInt>(data));
    if (isFloat()) return Value(-std::get<double>(data));
    if (isBool()) return Value(-toBigInt());
    throw std::runtime_error("Unary - unsupported for this type");
}

Value Value::operator+() const {
    if (isInt() || isFloat() || isBool()) return *this;
    throw std::runtime_error("Unary + unsupported for this type");
}

Value Value::operator!() const {
    return Value(!toBool());
}

bool Value::operator==(const Value& other) const {
    if (type() == NONE && other.type() == NONE) return true;
    if (type() == NONE || other.type() == NONE) return false;
    if (isString() || other.isString()) {
        if (isString() && other.isString()) return std::get<std::string>(data) == std::get<std::string>(other.data);
        return false;
    }
    if (isFloat() || other.isFloat()) {
        return toDouble() == other.toDouble();
    }
    return toBigInt() == other.toBigInt();
}

bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}

bool Value::operator<(const Value& other) const {
    if (isString() && other.isString()) return std::get<std::string>(data) < std::get<std::string>(other.data);
    if (isFloat() || other.isFloat()) return toDouble() < other.toDouble();
    return toBigInt() < other.toBigInt();
}

bool Value::operator<=(const Value& other) const {
    if (isString() && other.isString()) return std::get<std::string>(data) <= std::get<std::string>(other.data);
    if (isFloat() || other.isFloat()) return toDouble() <= other.toDouble();
    return toBigInt() <= other.toBigInt();
}

bool Value::operator>(const Value& other) const { return !(*this <= other); }
bool Value::operator>=(const Value& other) const { return !(*this < other); }

Value Value::operator+(const Value& other) const {
    if (isString() && other.isString()) return Value(std::get<std::string>(data) + std::get<std::string>(other.data));
    if (isFloat() || other.isFloat()) return Value(toDouble() + other.toDouble());
    return Value(toBigInt() + other.toBigInt());
}

Value Value::operator-(const Value& other) const {
    if (isFloat() || other.isFloat()) return Value(toDouble() - other.toDouble());
    return Value(toBigInt() - other.toBigInt());
}

Value Value::operator*(const Value& other) const {
    if (isString() && (other.isInt() || other.isBool())) {
        std::string res = "";
        BigInt count = other.toBigInt();
        std::string s = std::get<std::string>(data);
        for (BigInt i(0); i < count; i = i + BigInt(1)) res += s;
        return Value(res);
    }
    if ((isInt() || isBool()) && other.isString()) {
        return other * (*this);
    }
    if (isFloat() || other.isFloat()) return Value(toDouble() * other.toDouble());
    return Value(toBigInt() * other.toBigInt());
}

Value Value::operator/(const Value& other) const {
    return Value(toDouble() / other.toDouble());
}

Value Value::intDiv(const Value& other) const {
    if (isFloat() || other.isFloat()) {
        return Value(BigInt( (long long)std::floor(toDouble() / other.toDouble()) ));
    }
    return Value(toBigInt() / other.toBigInt());
}

Value Value::operator%(const Value& other) const {
    if (isFloat() || other.isFloat()) {
        double a = toDouble();
        double b = other.toDouble();
        double q = std::floor(a / b);
        return Value(a - q * b);
    }
    return Value(toBigInt() % other.toBigInt());
}

Value& Value::operator+=(const Value& other) { *this = *this + other; return *this; }
Value& Value::operator-=(const Value& other) { *this = *this - other; return *this; }
Value& Value::operator*=(const Value& other) { *this = *this * other; return *this; }
Value& Value::operator/=(const Value& other) { *this = *this / other; return *this; }
Value& Value::operator%=(const Value& other) { *this = *this % other; return *this; }
