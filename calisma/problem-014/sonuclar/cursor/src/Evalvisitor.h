#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"

#include "BigInt.h"
#include "Value.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class EvalVisitor : public Python3ParserBaseVisitor {
public:
	// =====================================================================
	//  Program / statements
	// =====================================================================
	std::any visitFile_input(Python3Parser::File_inputContext *ctx) override {
		for (auto *stmt : ctx->stmt()) {
			visit(stmt);
			if (flow_ != Flow::Normal) break; // defensive; top level stays Normal
		}
		return {};
	}

	std::any visitFuncdef(Python3Parser::FuncdefContext *ctx) override {
		functions_[ctx->NAME()->getText()] = ctx;
		return {};
	}

	std::any visitStmt(Python3Parser::StmtContext *ctx) override {
		if (ctx->simple_stmt()) return visit(ctx->simple_stmt());
		return visit(ctx->compound_stmt());
	}

	std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override {
		return visit(ctx->small_stmt());
	}

	std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override {
		if (ctx->expr_stmt()) return visit(ctx->expr_stmt());
		return visit(ctx->flow_stmt());
	}

	std::any visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) override {
		if (ctx->if_stmt()) return visit(ctx->if_stmt());
		if (ctx->while_stmt()) return visit(ctx->while_stmt());
		return visit(ctx->funcdef());
	}

	std::any visitSuite(Python3Parser::SuiteContext *ctx) override {
		if (ctx->simple_stmt()) {
			visit(ctx->simple_stmt());
			return {};
		}
		for (auto *stmt : ctx->stmt()) {
			visit(stmt);
			if (flow_ != Flow::Normal) break;
		}
		return {};
	}

	// ---- flow-control statements --------------------------------------
	std::any visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) override {
		if (ctx->break_stmt()) return visit(ctx->break_stmt());
		if (ctx->continue_stmt()) return visit(ctx->continue_stmt());
		return visit(ctx->return_stmt());
	}

	std::any visitBreak_stmt(Python3Parser::Break_stmtContext *) override {
		flow_ = Flow::Break;
		return {};
	}

	std::any visitContinue_stmt(Python3Parser::Continue_stmtContext *) override {
		flow_ = Flow::Continue;
		return {};
	}

	std::any visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) override {
		returnValue_ = ctx->testlist() ? evalTestlist(ctx->testlist()) : Value::makeNone();
		flow_ = Flow::Return;
		return {};
	}

	std::any visitIf_stmt(Python3Parser::If_stmtContext *ctx) override {
		auto tests = ctx->test();
		auto suites = ctx->suite();
		for (size_t i = 0; i < tests.size(); ++i) {
			if (eval(tests[i]).truth()) {
				visit(suites[i]);
				return {};
			}
		}
		if (suites.size() > tests.size()) // trailing else
			visit(suites.back());
		return {};
	}

	std::any visitWhile_stmt(Python3Parser::While_stmtContext *ctx) override {
		while (eval(ctx->test()).truth()) {
			visit(ctx->suite());
			if (flow_ == Flow::Break) {
				flow_ = Flow::Normal;
				break;
			}
			if (flow_ == Flow::Continue) {
				flow_ = Flow::Normal;
				continue;
			}
			if (flow_ == Flow::Return) break; // propagate upward
		}
		return {};
	}

	// =====================================================================
	//  Assignment / expression statement
	// =====================================================================
	std::any visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) override {
		auto testlists = ctx->testlist();

		if (ctx->augassign()) {
			std::string name = targetName(testlists[0]->test(0));
			Value cur = readVar(name);
			Value rhs = evalTestlist(testlists[1]);
			assignVar(name, binaryArith(cur, augOp(ctx->augassign()), rhs));
			return {};
		}

		if (testlists.size() == 1) {
			evalTestlist(testlists[0]); // expression statement, evaluated for effects
			return {};
		}

		// one or more '=' : last testlist is the source, the rest are targets
		size_t k = testlists.size() - 1;
		auto *src = testlists[k];

		if (k == 1) {
			auto *tgt = testlists[0];
			size_t m = tgt->test().size();
			size_t n = src->test().size();
			if (m > 1 && n == m) {
				// sequential unpacking: a, b = e1, e2  ->  a = e1; b = e2
				for (size_t i = 0; i < m; ++i)
					assignVar(targetName(tgt->test(i)), eval(src->test(i)));
			} else if (m > 1 && n == 1) {
				unpackInto(tgt, eval(src->test(0)));
			} else {
				assignVar(targetName(tgt->test(0)), evalTestlist(src));
			}
			return {};
		}

		// chained assignment: a = b = ... = value
		Value value = evalTestlist(src);
		for (size_t t = 0; t < k; ++t) {
			auto *tgt = testlists[t];
			if (tgt->test().size() == 1)
				assignVar(targetName(tgt->test(0)), value);
			else
				unpackInto(tgt, value);
		}
		return {};
	}

	// =====================================================================
	//  Expressions
	// =====================================================================
	std::any visitTest(Python3Parser::TestContext *ctx) override {
		return visit(ctx->or_test());
	}

	std::any visitOr_test(Python3Parser::Or_testContext *ctx) override {
		auto operands = ctx->and_test();
		if (operands.size() == 1) return visit(operands[0]);
		for (auto *op : operands)
			if (eval(op).truth()) return Value::makeBool(true); // short circuit
		return Value::makeBool(false);
	}

	std::any visitAnd_test(Python3Parser::And_testContext *ctx) override {
		auto operands = ctx->not_test();
		if (operands.size() == 1) return visit(operands[0]);
		for (auto *op : operands)
			if (!eval(op).truth()) return Value::makeBool(false); // short circuit
		return Value::makeBool(true);
	}

	std::any visitNot_test(Python3Parser::Not_testContext *ctx) override {
		if (ctx->NOT()) return Value::makeBool(!eval(ctx->not_test()).truth());
		return visit(ctx->comparison());
	}

	std::any visitComparison(Python3Parser::ComparisonContext *ctx) override {
		auto operands = ctx->arith_expr();
		if (operands.size() == 1) return visit(operands[0]);

		auto ops = ctx->comp_op();
		// Chained comparison with short-circuit; each operand evaluated once.
		Value prev = eval(operands[0]);
		bool result = true;
		for (size_t i = 0; i < ops.size(); ++i) {
			Value cur = eval(operands[i + 1]);
			if (!compareOp(prev, ops[i], cur)) {
				result = false;
				break;
			}
			prev = std::move(cur);
		}
		return Value::makeBool(result);
	}

	std::any visitArith_expr(Python3Parser::Arith_exprContext *ctx) override {
		auto terms = ctx->term();
		Value acc = eval(terms[0]);
		auto ops = ctx->addorsub_op();
		for (size_t i = 0; i < ops.size(); ++i) {
			Op op = ops[i]->ADD() ? Op::Add : Op::Sub;
			acc = binaryArith(acc, op, eval(terms[i + 1]));
		}
		return acc;
	}

	std::any visitTerm(Python3Parser::TermContext *ctx) override {
		auto factors = ctx->factor();
		Value acc = eval(factors[0]);
		auto ops = ctx->muldivmod_op();
		for (size_t i = 0; i < ops.size(); ++i) {
			acc = binaryArith(acc, mulOp(ops[i]), eval(factors[i + 1]));
		}
		return acc;
	}

	std::any visitFactor(Python3Parser::FactorContext *ctx) override {
		if (ctx->atom_expr()) return visit(ctx->atom_expr());
		Value inner = eval(ctx->factor());
		if (ctx->MINUS()) return negate(inner);
		return unaryPlus(inner);
	}

	std::any visitAtom_expr(Python3Parser::Atom_exprContext *ctx) override {
		if (ctx->trailer() == nullptr) return visit(ctx->atom());
		// function call: atom is the function NAME
		std::string name = ctx->atom()->NAME()->getText();
		return callFunction(name, ctx->trailer()->arglist());
	}

	std::any visitAtom(Python3Parser::AtomContext *ctx) override {
		if (ctx->NAME()) return readVar(ctx->NAME()->getText());
		if (ctx->NUMBER()) return parseNumber(ctx->NUMBER()->getText());
		if (ctx->NONE()) return Value::makeNone();
		if (ctx->TRUE()) return Value::makeBool(true);
		if (ctx->FALSE()) return Value::makeBool(false);
		if (ctx->format_string()) return visit(ctx->format_string());
		if (ctx->test()) return visit(ctx->test()); // ( test )
		// STRING+  ->  adjacent literal concatenation
		std::string s;
		for (auto *tok : ctx->STRING()) s += decodeStringLiteral(tok->getText());
		return Value::makeStr(std::move(s));
	}

	std::any visitFormat_string(Python3Parser::Format_stringContext *ctx) override {
		std::string result;
		for (auto *child : ctx->children) {
			if (auto *term = dynamic_cast<antlr4::tree::TerminalNode *>(child)) {
				if (term->getSymbol()->getType() == Python3Parser::FORMAT_STRING_LITERAL)
					result += decodeFormatLiteral(term->getText());
			} else if (auto *tl = dynamic_cast<Python3Parser::TestlistContext *>(child)) {
				result += scalarToString(evalTestlist(tl));
			}
		}
		return Value::makeStr(std::move(result));
	}

private:
	// =====================================================================
	//  Interpreter state
	// =====================================================================
	enum class Flow { Normal, Break, Continue, Return };
	enum class Op { Add, Sub, Mul, TrueDiv, FloorDiv, Mod };

	std::unordered_map<std::string, Value> globals_;
	std::vector<std::unordered_map<std::string, Value>> localStack_;
	std::unordered_map<std::string, Python3Parser::FuncdefContext *> functions_;

	Flow flow_ = Flow::Normal;
	Value returnValue_;

	// =====================================================================
	//  Small evaluation helpers
	// =====================================================================
	Value eval(antlr4::tree::ParseTree *node) {
		std::any r = visit(node);
		return std::move(*std::any_cast<Value>(&r)); // move out to avoid a copy
	}

	// A testlist yields a single value, or a tuple when it holds several tests.
	Value evalTestlist(Python3Parser::TestlistContext *ctx) {
		auto tests = ctx->test();
		if (tests.size() == 1) return eval(tests[0]);
		std::vector<Value> items;
		items.reserve(tests.size());
		for (auto *t : tests) items.push_back(eval(t));
		return Value::makeTuple(std::move(items));
	}

	// =====================================================================
	//  Variable scope handling
	// =====================================================================
	Value readVar(const std::string &name) {
		if (!localStack_.empty()) {
			auto &local = localStack_.back();
			auto it = local.find(name);
			if (it != local.end()) return it->second;
		}
		auto it = globals_.find(name);
		if (it != globals_.end()) return it->second;
		return Value::makeNone(); // undefined names are not expected in valid input
	}

	void assignVar(const std::string &name, const Value &value) {
		if (!localStack_.empty()) {
			auto &local = localStack_.back();
			if (local.count(name)) {          // existing local (incl. parameters)
				local[name] = value;
				return;
			}
			if (globals_.count(name)) {       // globals are writable without `global`
				globals_[name] = value;
				return;
			}
			local[name] = value;              // fresh function-local variable
			return;
		}
		globals_[name] = value;
	}

	// Extracts the plain variable name from an assignment target expression.
	std::string targetName(Python3Parser::TestContext *test) {
		auto *orTest = test->or_test();
		auto *andTest = orTest->and_test(0);
		auto *notTest = andTest->not_test(0);
		auto *comp = notTest->comparison();
		auto *arith = comp->arith_expr(0);
		auto *term = arith->term(0);
		auto *factor = term->factor(0);
		auto *atomExpr = factor->atom_expr();
		return atomExpr->atom()->NAME()->getText();
	}

	void unpackInto(Python3Parser::TestlistContext *targets, const Value &value) {
		auto tests = targets->test();
		if (value.isTuple()) {
			const auto &items = *value.tupleVal;
			for (size_t i = 0; i < tests.size() && i < items.size(); ++i)
				assignVar(targetName(tests[i]), items[i]);
		} else if (tests.size() == 1) {
			assignVar(targetName(tests[0]), value);
		}
	}

	// =====================================================================
	//  Operator helpers
	// =====================================================================
	static Op augOp(Python3Parser::AugassignContext *ctx) {
		if (ctx->ADD_ASSIGN()) return Op::Add;
		if (ctx->SUB_ASSIGN()) return Op::Sub;
		if (ctx->MULT_ASSIGN()) return Op::Mul;
		if (ctx->DIV_ASSIGN()) return Op::TrueDiv;
		if (ctx->IDIV_ASSIGN()) return Op::FloorDiv;
		return Op::Mod; // MOD_ASSIGN
	}

	static Op mulOp(Python3Parser::Muldivmod_opContext *ctx) {
		if (ctx->STAR()) return Op::Mul;
		if (ctx->DIV()) return Op::TrueDiv;
		if (ctx->IDIV()) return Op::FloorDiv;
		return Op::Mod; // MOD
	}

	static Value negate(const Value &v) {
		if (v.isBool()) return Value::makeInt(BigInt(v.boolVal ? -1 : 0));
		if (v.isInt()) return Value::makeInt(-v.intVal);
		if (v.isFloat()) return Value::makeFloat(-v.floatVal);
		return v;
	}

	static Value unaryPlus(const Value &v) {
		if (v.isBool()) return Value::makeInt(BigInt(v.boolVal ? 1 : 0));
		return v;
	}

	static Value repeatString(const std::string &s, const BigInt &countBig) {
		long long count = countBig.toLongLong();
		if (count <= 0) return Value::makeStr("");
		std::string r;
		r.reserve(s.size() * static_cast<size_t>(count));
		for (long long i = 0; i < count; ++i) r += s;
		return Value::makeStr(std::move(r));
	}

	Value binaryArith(const Value &a, Op op, const Value &b) {
		// string operations
		if (a.isStr() || b.isStr()) {
			if (op == Op::Add && a.isStr() && b.isStr())
				return Value::makeStr(a.strVal + b.strVal);
			if (op == Op::Mul) {
				if (a.isStr() && (b.isInt() || b.isBool())) return repeatString(a.strVal, b.asBigInt());
				if (b.isStr() && (a.isInt() || a.isBool())) return repeatString(b.strVal, a.asBigInt());
			}
			throw std::runtime_error("unsupported string operation");
		}

		if (op == Op::TrueDiv) return Value::makeFloat(a.asDouble() / b.asDouble());

		if (a.isFloat() || b.isFloat()) {
			double x = a.asDouble(), y = b.asDouble();
			switch (op) {
				case Op::Add: return Value::makeFloat(x + y);
				case Op::Sub: return Value::makeFloat(x - y);
				case Op::Mul: return Value::makeFloat(x * y);
				case Op::FloorDiv: return Value::makeFloat(std::floor(x / y));
				case Op::Mod: return Value::makeFloat(x - std::floor(x / y) * y);
				default: break;
			}
		}

		// Fast path for the common int-int case: operate on the stored BigInt
		// directly, avoiding the copies that asBigInt() would make.
		if (a.isInt() && b.isInt()) {
			const BigInt &x = a.intVal, &y = b.intVal;
			switch (op) {
				case Op::Add: return Value::makeInt(x + y);
				case Op::Sub: return Value::makeInt(x - y);
				case Op::Mul: return Value::makeInt(x * y);
				case Op::FloorDiv: return Value::makeInt(x / y);
				case Op::Mod: return Value::makeInt(x % y);
				default: break;
			}
		}

		BigInt x = a.asBigInt(), y = b.asBigInt();
		switch (op) {
			case Op::Add: return Value::makeInt(x + y);
			case Op::Sub: return Value::makeInt(x - y);
			case Op::Mul: return Value::makeInt(x * y);
			case Op::FloorDiv: return Value::makeInt(x / y);
			case Op::Mod: return Value::makeInt(x % y);
			default: break;
		}
		throw std::runtime_error("bad arithmetic operator");
	}

	static bool valueEquals(const Value &a, const Value &b) {
		if (a.isNone() || b.isNone()) return a.isNone() && b.isNone();
		if (a.isStr() || b.isStr()) return a.isStr() && b.isStr() && a.strVal == b.strVal;
		if (a.isTuple() || b.isTuple()) {
			if (!a.isTuple() || !b.isTuple()) return false;
			const auto &x = *a.tupleVal;
			const auto &y = *b.tupleVal;
			if (x.size() != y.size()) return false;
			for (size_t i = 0; i < x.size(); ++i)
				if (!valueEquals(x[i], y[i])) return false;
			return true;
		}
		if (a.isFloat() || b.isFloat()) return a.asDouble() == b.asDouble();
		return a.asBigInt() == b.asBigInt();
	}

	bool compareOp(const Value &a, Python3Parser::Comp_opContext *op, const Value &b) {
		if (op->EQUALS()) return valueEquals(a, b);
		if (op->NOT_EQ_2()) return !valueEquals(a, b);

		int cmp;
		if (a.isStr() && b.isStr()) {
			int c = a.strVal.compare(b.strVal);
			cmp = (c < 0) ? -1 : (c > 0 ? 1 : 0);
		} else if (a.isNumeric() && b.isNumeric()) {
			if (a.isFloat() || b.isFloat()) {
				double x = a.asDouble(), y = b.asDouble();
				cmp = (x < y) ? -1 : (x > y ? 1 : 0);
			} else {
				cmp = a.asBigInt().compare(b.asBigInt());
			}
		} else {
			return false; // undefined comparison
		}

		if (op->LESS_THAN()) return cmp < 0;
		if (op->GREATER_THAN()) return cmp > 0;
		if (op->LT_EQ()) return cmp <= 0;
		if (op->GT_EQ()) return cmp >= 0;
		return false;
	}

	// =====================================================================
	//  Literals
	// =====================================================================
	static Value parseNumber(const std::string &text) {
		if (text.find('.') != std::string::npos)
			return Value::makeFloat(std::stod(text));
		return Value::makeInt(BigInt(text));
	}

	static std::string unescape(const std::string &s) {
		std::string r;
		r.reserve(s.size());
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '\\' && i + 1 < s.size()) {
				char n = s[++i];
				switch (n) {
					case 'n': r += '\n'; break;
					case 't': r += '\t'; break;
					case 'r': r += '\r'; break;
					case '\\': r += '\\'; break;
					case '\'': r += '\''; break;
					case '"': r += '"'; break;
					case '0': r += '\0'; break;
					case 'a': r += '\a'; break;
					case 'b': r += '\b'; break;
					case 'f': r += '\f'; break;
					case 'v': r += '\v'; break;
					default: r += '\\'; r += n; break;
				}
			} else {
				r += s[i];
			}
		}
		return r;
	}

	// Strips the surrounding quotes and resolves escape sequences.
	static std::string decodeStringLiteral(const std::string &tok) {
		if (tok.size() < 2) return "";
		return unescape(tok.substr(1, tok.size() - 2));
	}

	// Resolves {{ and }} escapes plus backslash escapes inside an f-string piece.
	static std::string decodeFormatLiteral(const std::string &s) {
		std::string r;
		r.reserve(s.size());
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '{' && i + 1 < s.size() && s[i + 1] == '{') {
				r += '{';
				++i;
			} else if (s[i] == '}' && i + 1 < s.size() && s[i + 1] == '}') {
				r += '}';
				++i;
			} else if (s[i] == '\\' && i + 1 < s.size()) {
				std::string piece = unescape(s.substr(i, 2));
				r += piece;
				++i;
			} else {
				r += s[i];
			}
		}
		return r;
	}

	// =====================================================================
	//  Function calls & built-ins
	// =====================================================================
	struct Args {
		std::vector<Value> positional;
		std::vector<std::pair<std::string, Value>> keyword;
	};

	Args evalArgs(Python3Parser::ArglistContext *arglist) {
		Args args;
		if (!arglist) return args;
		for (auto *arg : arglist->argument()) {
			if (arg->ASSIGN()) {
				args.keyword.emplace_back(targetName(arg->test(0)), eval(arg->test(1)));
			} else {
				args.positional.push_back(eval(arg->test(0)));
			}
		}
		return args;
	}

	Value callFunction(const std::string &name, Python3Parser::ArglistContext *arglist) {
		if (name == "print") return builtinPrint(arglist);
		if (name == "int" || name == "float" || name == "str" || name == "bool")
			return builtinConvert(name, arglist);
		return callUserFunction(name, arglist);
	}

	Value builtinPrint(Python3Parser::ArglistContext *arglist) {
		Args args = evalArgs(arglist);
		for (size_t i = 0; i < args.positional.size(); ++i) {
			if (i) std::cout << ' ';
			std::cout << toDisplayString(args.positional[i]);
		}
		std::cout << '\n';
		return Value::makeNone();
	}

	Value builtinConvert(const std::string &name, Python3Parser::ArglistContext *arglist) {
		Args args = evalArgs(arglist);
		const Value &v = args.positional[0];
		if (name == "int") return toInt(v);
		if (name == "float") return toFloat(v);
		if (name == "str") return Value::makeStr(toDisplayString(v));
		return Value::makeBool(v.truth()); // bool
	}

	static std::string trim(const std::string &s) {
		size_t a = 0, b = s.size();
		while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
		while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
		return s.substr(a, b - a);
	}

	static Value toInt(const Value &v) {
		if (v.isInt()) return v;
		if (v.isBool()) return Value::makeInt(BigInt(v.boolVal ? 1 : 0));
		if (v.isFloat()) return Value::makeInt(BigInt::fromDouble(v.floatVal));
		if (v.isStr()) return Value::makeInt(BigInt(trim(v.strVal)));
		return Value::makeInt(BigInt());
	}

	static Value toFloat(const Value &v) {
		if (v.isFloat()) return v;
		if (v.isBool()) return Value::makeFloat(v.boolVal ? 1.0 : 0.0);
		if (v.isInt()) return Value::makeFloat(v.intVal.toDouble());
		if (v.isStr()) return Value::makeFloat(std::stod(trim(v.strVal)));
		return Value::makeFloat(0.0);
	}

	Value callUserFunction(const std::string &name, Python3Parser::ArglistContext *arglist) {
		auto fit = functions_.find(name);
		if (fit == functions_.end()) throw std::runtime_error("undefined function: " + name);
		Python3Parser::FuncdefContext *fdef = fit->second;

		std::vector<std::string> params;
		std::vector<Python3Parser::TestContext *> defaults;
		parseParams(fdef, params, defaults);

		// Arguments are evaluated in the caller's scope, before the new frame.
		Args args = evalArgs(arglist);

		std::unordered_map<std::string, Value> frame;
		for (size_t i = 0; i < args.positional.size() && i < params.size(); ++i)
			frame[params[i]] = args.positional[i];
		for (auto &kv : args.keyword) frame[kv.first] = kv.second;
		for (size_t i = 0; i < params.size(); ++i)
			if (!frame.count(params[i]) && defaults[i])
				frame[params[i]] = eval(defaults[i]);

		localStack_.push_back(std::move(frame));
		flow_ = Flow::Normal;
		visit(fdef->suite());
		Value result = (flow_ == Flow::Return) ? returnValue_ : Value::makeNone();
		flow_ = Flow::Normal;
		returnValue_ = Value::makeNone();
		localStack_.pop_back();
		return result;
	}

	static void parseParams(Python3Parser::FuncdefContext *fdef,
							std::vector<std::string> &params,
							std::vector<Python3Parser::TestContext *> &defaults) {
		auto *parameters = fdef->parameters();
		auto *tal = parameters->typedargslist();
		if (!tal) return;
		for (auto *tfp : tal->tfpdef()) params.push_back(tfp->NAME()->getText());
		defaults.assign(params.size(), nullptr);
		auto tests = tal->test();
		size_t k = tests.size();
		size_t m = params.size();
		for (size_t j = 0; j < k; ++j) defaults[m - k + j] = tests[j];
	}
};

#endif // PYTHON_INTERPRETER_EVALVISITOR_H
