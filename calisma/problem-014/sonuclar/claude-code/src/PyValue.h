#pragma once
#ifndef PYTHON_INTERPRETER_PYVALUE_H
#define PYTHON_INTERPRETER_PYVALUE_H

#include "BigInt.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// Runtime value of the interpreter. A tagged union over the supported Python
// types. Tuples are held behind a shared_ptr so copying a Value stays cheap.
enum class PyType { NONE, BOOL, INT, FLOAT, STR, TUPLE };

struct Value {
    PyType type = PyType::NONE;
    bool b = false;
    BigInt i;
    double f = 0.0;
    std::string s;
    std::shared_ptr<std::vector<Value>> tup;
};

// ---- constructors ----
inline Value makeNone() { return Value{}; }
inline Value makeBool(bool v) {
    Value r;
    r.type = PyType::BOOL;
    r.b = v;
    return r;
}
inline Value makeInt(const BigInt &v) {
    Value r;
    r.type = PyType::INT;
    r.i = v;
    return r;
}
inline Value makeInt(BigInt &&v) {
    Value r;
    r.type = PyType::INT;
    r.i = std::move(v);
    return r;
}
inline Value makeFloat(double v) {
    Value r;
    r.type = PyType::FLOAT;
    r.f = v;
    return r;
}
inline Value makeStr(const std::string &v) {
    Value r;
    r.type = PyType::STR;
    r.s = v;
    return r;
}
inline Value makeStr(std::string &&v) {
    Value r;
    r.type = PyType::STR;
    r.s = std::move(v);
    return r;
}
inline Value makeTuple(std::vector<Value> elems) {
    Value r;
    r.type = PyType::TUPLE;
    r.tup = std::make_shared<std::vector<Value>>(std::move(elems));
    return r;
}

// ---- classification helpers ----
inline bool isIntKind(const Value &v) { return v.type == PyType::INT || v.type == PyType::BOOL; }
inline bool isNumber(const Value &v) {
    return v.type == PyType::INT || v.type == PyType::BOOL || v.type == PyType::FLOAT;
}

inline BigInt asBig(const Value &v) {
    if (v.type == PyType::BOOL) return BigInt(v.b ? 1 : 0);
    return v.i; // INT
}
inline double asDouble(const Value &v) {
    switch (v.type) {
        case PyType::BOOL:
            return v.b ? 1.0 : 0.0;
        case PyType::INT:
            return v.i.toDouble();
        case PyType::FLOAT:
            return v.f;
        default:
            return 0.0;
    }
}

// ---- truthiness ----
inline bool toBool(const Value &v) {
    switch (v.type) {
        case PyType::NONE:
            return false;
        case PyType::BOOL:
            return v.b;
        case PyType::INT:
            return !v.i.isZero();
        case PyType::FLOAT:
            return v.f != 0.0;
        case PyType::STR:
            return !v.s.empty();
        case PyType::TUPLE:
            return v.tup && !v.tup->empty();
    }
    return false;
}

inline std::string formatFloat(double f) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", f);
    return std::string(buf);
}

// str()/print() rendering of a value (strings are shown raw).
std::string toStr(const Value &v);

// repr() rendering used for tuple elements (strings get quoted).
inline std::string reprValue(const Value &v) {
    if (v.type == PyType::STR) return "'" + v.s + "'";
    return toStr(v);
}

inline std::string toStr(const Value &v) {
    switch (v.type) {
        case PyType::NONE:
            return "None";
        case PyType::BOOL:
            return v.b ? "True" : "False";
        case PyType::INT:
            return v.i.toString();
        case PyType::FLOAT:
            return formatFloat(v.f);
        case PyType::STR:
            return v.s;
        case PyType::TUPLE: {
            std::string out = "(";
            const auto &e = *v.tup;
            for (size_t k = 0; k < e.size(); k++) {
                if (k) out += ", ";
                out += reprValue(e[k]);
            }
            if (e.size() == 1) out += ",";
            out += ")";
            return out;
        }
    }
    return "";
}

// ---- equality (used by == and !=) ----
inline bool valueEquals(const Value &a, const Value &b) {
    if (isNumber(a) && isNumber(b)) {
        if (isIntKind(a) && isIntKind(b)) return asBig(a) == asBig(b);
        return asDouble(a) == asDouble(b);
    }
    if (a.type == PyType::STR && b.type == PyType::STR) return a.s == b.s;
    if (a.type == PyType::NONE && b.type == PyType::NONE) return true;
    return false;
}

// Operator codes (avoid per-operation string allocation/comparison).
enum class BinOp { ADD, SUB, MUL, DIV, FLOORDIV, MOD };
enum class CmpOp { LT, GT, EQ, GE, LE, NE };

// ---- comparison operators (returns a bool) ----
inline bool compareOp(const Value &a, CmpOp op, const Value &b) {
    if (op == CmpOp::EQ) return valueEquals(a, b);
    if (op == CmpOp::NE) return !valueEquals(a, b);
    // ordering: < > <= >=
    int c; // sign of (a - b): -1, 0, or 1
    if (a.type == PyType::STR && b.type == PyType::STR) {
        int cc = a.s.compare(b.s);
        c = (cc < 0) ? -1 : (cc > 0 ? 1 : 0);
    } else if (isIntKind(a) && isIntKind(b)) {
        c = asBig(a).cmp(asBig(b));
    } else {
        double x = asDouble(a), y = asDouble(b);
        c = (x < y) ? -1 : (x > y ? 1 : 0);
    }
    switch (op) {
        case CmpOp::LT:
            return c < 0;
        case CmpOp::GT:
            return c > 0;
        case CmpOp::LE:
            return c <= 0;
        case CmpOp::GE:
            return c >= 0;
        default:
            return false;
    }
}

// ---- arithmetic ----
inline Value binaryOp(const Value &a, BinOp op, const Value &b) {
    switch (op) {
        case BinOp::ADD:
            if (a.type == PyType::STR && b.type == PyType::STR) return makeStr(a.s + b.s);
            if (a.type == PyType::FLOAT || b.type == PyType::FLOAT)
                return makeFloat(asDouble(a) + asDouble(b));
            return makeInt(asBig(a) + asBig(b));
        case BinOp::SUB:
            if (a.type == PyType::FLOAT || b.type == PyType::FLOAT)
                return makeFloat(asDouble(a) - asDouble(b));
            return makeInt(asBig(a) - asBig(b));
        case BinOp::MUL: {
            if (a.type == PyType::STR && isIntKind(b)) {
                long long n = asBig(b).toLL();
                std::string out;
                for (long long k = 0; k < n; k++) out += a.s;
                return makeStr(std::move(out));
            }
            if (isIntKind(a) && b.type == PyType::STR) {
                long long n = asBig(a).toLL();
                std::string out;
                for (long long k = 0; k < n; k++) out += b.s;
                return makeStr(std::move(out));
            }
            if (a.type == PyType::FLOAT || b.type == PyType::FLOAT)
                return makeFloat(asDouble(a) * asDouble(b));
            return makeInt(asBig(a) * asBig(b));
        }
        case BinOp::DIV:
            return makeFloat(asDouble(a) / asDouble(b));
        case BinOp::FLOORDIV:
            if (a.type == PyType::FLOAT || b.type == PyType::FLOAT)
                return makeFloat(std::floor(asDouble(a) / asDouble(b)));
            return makeInt(asBig(a).floordiv(asBig(b)));
        case BinOp::MOD:
            if (a.type == PyType::FLOAT || b.type == PyType::FLOAT) {
                double x = asDouble(a), y = asDouble(b);
                return makeFloat(x - std::floor(x / y) * y);
            }
            return makeInt(asBig(a).mod(asBig(b)));
    }
    return makeNone();
}

// ---- unary +/- ----
inline Value negate(const Value &v) {
    if (v.type == PyType::FLOAT) return makeFloat(-v.f);
    return makeInt(-asBig(v)); // INT or BOOL -> INT
}
inline Value unaryPlus(const Value &v) {
    if (v.type == PyType::BOOL) return makeInt(asBig(v));
    return v; // INT / FLOAT unchanged
}

// ---- explicit conversions (builtins int/float/str/bool) ----
inline Value convInt(const Value &v) {
    switch (v.type) {
        case PyType::INT:
            return v;
        case PyType::BOOL:
            return makeInt(BigInt(v.b ? 1 : 0));
        case PyType::FLOAT: {
            double t = std::trunc(v.f); // truncate toward zero
            if (t >= -9.2e18 && t <= 9.2e18) return makeInt(BigInt(static_cast<long long>(t)));
            // fall back through string for very large magnitudes
            return makeInt(BigInt::fromString(formatFloat(t).substr(0, formatFloat(t).find('.'))));
        }
        case PyType::STR:
            return makeInt(BigInt::fromString(v.s));
        default:
            return makeInt(BigInt(0));
    }
}
inline Value convFloat(const Value &v) {
    switch (v.type) {
        case PyType::FLOAT:
            return v;
        case PyType::BOOL:
            return makeFloat(v.b ? 1.0 : 0.0);
        case PyType::INT:
            return makeFloat(v.i.toDouble());
        case PyType::STR:
            return makeFloat(std::stod(v.s));
        default:
            return makeFloat(0.0);
    }
}
inline Value convStr(const Value &v) { return makeStr(toStr(v)); }
inline Value convBool(const Value &v) { return makeBool(toBool(v)); }

#endif // PYTHON_INTERPRETER_PYVALUE_H
