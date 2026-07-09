#pragma once
#ifndef PYTHON_INTERPRETER_VALUE_H
#define PYTHON_INTERPRETER_VALUE_H

#include "BigInt.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// A dynamically typed value in the interpreter.
// Tuples are held via shared_ptr so copying a Value stays cheap and avoids the
// self-referential storage problem.
class Value {
public:
	enum class Type { None, Bool, Int, Float, Str, Tuple };

	Type type = Type::None;
	bool boolVal = false;
	BigInt intVal;
	double floatVal = 0.0;
	std::string strVal;
	std::shared_ptr<std::vector<Value>> tupleVal;

	Value() = default;

	// ---- factory helpers ----------------------------------------------
	static Value makeNone() { return Value(); }
	static Value makeBool(bool b) {
		Value v;
		v.type = Type::Bool;
		v.boolVal = b;
		return v;
	}
	static Value makeInt(BigInt b) {
		Value v;
		v.type = Type::Int;
		v.intVal = std::move(b);
		return v;
	}
	static Value makeFloat(double d) {
		Value v;
		v.type = Type::Float;
		v.floatVal = d;
		return v;
	}
	static Value makeStr(std::string s) {
		Value v;
		v.type = Type::Str;
		v.strVal = std::move(s);
		return v;
	}
	static Value makeTuple(std::vector<Value> items) {
		Value v;
		v.type = Type::Tuple;
		v.tupleVal = std::make_shared<std::vector<Value>>(std::move(items));
		return v;
	}

	// ---- predicates ----------------------------------------------------
	bool isNone() const { return type == Type::None; }
	bool isBool() const { return type == Type::Bool; }
	bool isInt() const { return type == Type::Int; }
	bool isFloat() const { return type == Type::Float; }
	bool isStr() const { return type == Type::Str; }
	bool isTuple() const { return type == Type::Tuple; }
	bool isNumeric() const { return type == Type::Bool || type == Type::Int || type == Type::Float; }

	// ---- coercions -----------------------------------------------------
	bool truth() const {
		switch (type) {
			case Type::None: return false;
			case Type::Bool: return boolVal;
			case Type::Int: return !intVal.isZero();
			case Type::Float: return floatVal != 0.0;
			case Type::Str: return !strVal.empty();
			case Type::Tuple: return tupleVal && !tupleVal->empty();
		}
		return false;
	}

	BigInt asBigInt() const {
		if (type == Type::Bool) return BigInt(boolVal ? 1 : 0);
		if (type == Type::Int) return intVal;
		if (type == Type::Float) return BigInt::fromDouble(floatVal);
		return BigInt();
	}

	double asDouble() const {
		if (type == Type::Bool) return boolVal ? 1.0 : 0.0;
		if (type == Type::Int) return intVal.toDouble();
		if (type == Type::Float) return floatVal;
		return 0.0;
	}
};

// Formats a double with the fixed 6-decimal representation the spec mandates.
inline std::string formatFloat(double d) {
	char buf[512];
	std::snprintf(buf, sizeof(buf), "%.6f", d);
	return std::string(buf);
}

// Scalar string form (as produced by str()/print for non-tuple values).
inline std::string scalarToString(const Value &v) {
	switch (v.type) {
		case Value::Type::None: return "None";
		case Value::Type::Bool: return v.boolVal ? "True" : "False";
		case Value::Type::Int: return v.intVal.toString();
		case Value::Type::Float: return formatFloat(v.floatVal);
		case Value::Type::Str: return v.strVal;
		default: return "";
	}
}

// repr() form, used for tuple elements (strings are quoted, like Python).
inline std::string reprToString(const Value &v);

inline std::string tupleToString(const Value &v) {
	std::string s = "(";
	const auto &items = *v.tupleVal;
	for (size_t i = 0; i < items.size(); ++i) {
		if (i) s += ", ";
		s += reprToString(items[i]);
	}
	if (items.size() == 1) s += ",";
	s += ")";
	return s;
}

inline std::string reprToString(const Value &v) {
	if (v.isStr()) return "'" + v.strVal + "'";
	if (v.isTuple()) return tupleToString(v);
	return scalarToString(v);
}

// print()/str() rendering: tuples get their repr, scalars their plain form.
inline std::string toDisplayString(const Value &v) {
	if (v.isTuple()) return tupleToString(v);
	return scalarToString(v);
}

#endif // PYTHON_INTERPRETER_VALUE_H
