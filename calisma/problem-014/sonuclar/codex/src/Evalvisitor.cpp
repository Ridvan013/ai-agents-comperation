#include "Evalvisitor.h"

#include <boost/multiprecision/cpp_int.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using BigInt = boost::multiprecision::cpp_int;

namespace {

struct TupleData;
struct Function;
struct Frame;

class Value {
public:
	using Storage = std::variant<std::monostate, bool, BigInt, double, std::string,
	                             std::shared_ptr<TupleData>, std::shared_ptr<Function>>;

	Value() = default;

	static Value none() {
		return Value();
	}

	static Value boolean(bool value) {
		return Value(value);
	}

	static Value integer(const BigInt &value) {
		return Value(value);
	}

	static Value floating(double value) {
		return Value(value);
	}

	static Value string(std::string value) {
		return Value(std::move(value));
	}

	static Value tuple(std::vector<Value> values);

	static Value function(std::shared_ptr<Function> function) {
		return Value(std::move(function));
	}

	bool isNone() const {
		return std::holds_alternative<std::monostate>(data_);
	}

	bool isBool() const {
		return std::holds_alternative<bool>(data_);
	}

	bool isInt() const {
		return std::holds_alternative<BigInt>(data_);
	}

	bool isFloat() const {
		return std::holds_alternative<double>(data_);
	}

	bool isString() const {
		return std::holds_alternative<std::string>(data_);
	}

	bool isTuple() const {
		return std::holds_alternative<std::shared_ptr<TupleData>>(data_);
	}

	bool isFunction() const {
		return std::holds_alternative<std::shared_ptr<Function>>(data_);
	}

	bool asBool() const {
		return std::get<bool>(data_);
	}

	const BigInt &asInt() const {
		return std::get<BigInt>(data_);
	}

	double asFloat() const {
		return std::get<double>(data_);
	}

	const std::string &asString() const {
		return std::get<std::string>(data_);
	}

	const std::shared_ptr<TupleData> &asTuple() const {
		return std::get<std::shared_ptr<TupleData>>(data_);
	}

	const std::shared_ptr<Function> &asFunction() const {
		return std::get<std::shared_ptr<Function>>(data_);
	}

private:
	explicit Value(bool value) : data_(value) {}
	explicit Value(const BigInt &value) : data_(value) {}
	explicit Value(double value) : data_(value) {}
	explicit Value(std::string value) : data_(std::move(value)) {}
	explicit Value(std::shared_ptr<TupleData> value) : data_(std::move(value)) {}
	explicit Value(std::shared_ptr<Function> value) : data_(std::move(value)) {}

	Storage data_;
};

struct TupleData {
	std::vector<Value> elements;
};

Value Value::tuple(std::vector<Value> values) {
	return Value(std::make_shared<TupleData>(TupleData{std::move(values)}));
}

enum class BuiltinKind {
	Print,
	Int,
	Float,
	Str,
	Bool,
	User,
};

struct Parameter {
	std::string name;
	std::optional<Value> defaultValue;
};

struct Function {
	BuiltinKind kind = BuiltinKind::User;
	std::string name;
	std::vector<Parameter> parameters;
	Python3Parser::SuiteContext *body = nullptr;
	std::shared_ptr<Frame> closure;
};

struct Frame {
	std::unordered_map<std::string, Value> values;
	std::shared_ptr<Frame> parent;
	bool isGlobal = false;
};

struct CallArgument {
	std::optional<std::string> keyword;
	Value value;
};

struct ReturnSignal {
	Value value;
};

struct BreakSignal {};
struct ContinueSignal {};

BigInt parseBigInt(const std::string &text) {
	std::stringstream stream(text);
	BigInt value = 0;
	stream >> value;
	return value;
}

std::string bigIntToString(const BigInt &value) {
	std::ostringstream stream;
	stream << value;
	return stream.str();
}

std::string formatFloat(double value) {
	std::ostringstream stream;
	stream << std::fixed << std::setprecision(6) << value;
	return stream.str();
}

BigInt truncDoubleToBigInt(double value) {
	const double truncated = value >= 0 ? std::floor(value) : std::ceil(value);
	return parseBigInt(formatFloat(truncated));
}

std::string unquoteLiteral(const std::string &tokenText) {
	if (tokenText.size() >= 2) {
		return tokenText.substr(1, tokenText.size() - 2);
	}
	return tokenText;
}

std::string decodeFormatLiteral(std::string text) {
	std::string decoded;
	decoded.reserve(text.size());
	for (std::size_t index = 0; index < text.size(); ++index) {
		if (index + 1 < text.size() && text[index] == '{' && text[index + 1] == '{') {
			decoded.push_back('{');
			++index;
		} else if (index + 1 < text.size() && text[index] == '}' && text[index + 1] == '}') {
			decoded.push_back('}');
			++index;
		} else {
			decoded.push_back(text[index]);
		}
	}
	return decoded;
}

bool isNumeric(const Value &value) {
	return value.isBool() || value.isInt() || value.isFloat();
}

BigInt toBigIntNumeric(const Value &value) {
	if (value.isBool()) {
		return value.asBool() ? BigInt(1) : BigInt(0);
	}
	if (value.isInt()) {
		return value.asInt();
	}
	return truncDoubleToBigInt(value.asFloat());
}

double toDoubleNumeric(const Value &value) {
	if (value.isBool()) {
		return value.asBool() ? 1.0 : 0.0;
	}
	if (value.isInt()) {
		return value.asInt().convert_to<double>();
	}
	return value.asFloat();
}

bool truthy(const Value &value) {
	if (value.isNone()) {
		return false;
	}
	if (value.isBool()) {
		return value.asBool();
	}
	if (value.isInt()) {
		return value.asInt() != 0;
	}
	if (value.isFloat()) {
		return value.asFloat() != 0.0;
	}
	if (value.isString()) {
		return !value.asString().empty();
	}
	if (value.isTuple()) {
		return !value.asTuple()->elements.empty();
	}
	return true;
}

BigInt floorDivInt(const BigInt &left, const BigInt &right) {
	BigInt quotient = left / right;
	BigInt remainder = left % right;
	if (remainder != 0 && ((remainder > 0) != (right > 0))) {
		--quotient;
	}
	return quotient;
}

double floorDivFloat(double left, double right) {
	return std::floor(left / right);
}

Value applyBinaryNumeric(const Value &left, const Value &right, char op) {
	if (left.isFloat() || right.isFloat()) {
		const double lhs = toDoubleNumeric(left);
		const double rhs = toDoubleNumeric(right);
		switch (op) {
		case '+':
			return Value::floating(lhs + rhs);
		case '-':
			return Value::floating(lhs - rhs);
		case '*':
			return Value::floating(lhs * rhs);
		default:
			break;
		}
	}

	const BigInt lhs = toBigIntNumeric(left);
	const BigInt rhs = toBigIntNumeric(right);
	switch (op) {
	case '+':
		return Value::integer(lhs + rhs);
	case '-':
		return Value::integer(lhs - rhs);
	case '*':
		return Value::integer(lhs * rhs);
	default:
		break;
	}
	return Value::none();
}

Value addValues(const Value &left, const Value &right) {
	if (left.isString() && right.isString()) {
		return Value::string(left.asString() + right.asString());
	}
	return applyBinaryNumeric(left, right, '+');
}

Value subValues(const Value &left, const Value &right) {
	return applyBinaryNumeric(left, right, '-');
}

Value mulValues(const Value &left, const Value &right) {
	if (left.isString() && (right.isBool() || right.isInt())) {
		const BigInt count = toBigIntNumeric(right);
		if (count <= 0) {
			return Value::string("");
		}
		const std::size_t repeats = count.convert_to<std::size_t>();
		std::string result;
		result.reserve(left.asString().size() * repeats);
		for (std::size_t index = 0; index < repeats; ++index) {
			result += left.asString();
		}
		return Value::string(std::move(result));
	}
	if (right.isString() && (left.isBool() || left.isInt())) {
		return mulValues(right, left);
	}
	return applyBinaryNumeric(left, right, '*');
}

Value divValues(const Value &left, const Value &right) {
	return Value::floating(toDoubleNumeric(left) / toDoubleNumeric(right));
}

Value floorDivValues(const Value &left, const Value &right) {
	if (left.isFloat() || right.isFloat()) {
		return Value::floating(floorDivFloat(toDoubleNumeric(left), toDoubleNumeric(right)));
	}
	return Value::integer(floorDivInt(toBigIntNumeric(left), toBigIntNumeric(right)));
}

Value modValues(const Value &left, const Value &right) {
	if (left.isFloat() || right.isFloat()) {
		const double divisor = toDoubleNumeric(right);
		const double quotient = floorDivFloat(toDoubleNumeric(left), divisor);
		return Value::floating(toDoubleNumeric(left) - quotient * divisor);
	}
	const BigInt lhs = toBigIntNumeric(left);
	const BigInt rhs = toBigIntNumeric(right);
	const BigInt quotient = floorDivInt(lhs, rhs);
	return Value::integer(lhs - quotient * rhs);
}

Value negateValue(const Value &value) {
	if (value.isFloat()) {
		return Value::floating(-value.asFloat());
	}
	return Value::integer(-toBigIntNumeric(value));
}

Value plusValue(const Value &value) {
	if (value.isFloat()) {
		return Value::floating(value.asFloat());
	}
	return Value::integer(toBigIntNumeric(value));
}

bool equalValues(const Value &left, const Value &right);

std::string reprString(const Value &value);

bool lessValues(const Value &left, const Value &right) {
	if (left.isString() && right.isString()) {
		return left.asString() < right.asString();
	}
	if (left.isTuple() && right.isTuple()) {
		const auto &lhs = left.asTuple()->elements;
		const auto &rhs = right.asTuple()->elements;
		const std::size_t limit = std::min(lhs.size(), rhs.size());
		for (std::size_t index = 0; index < limit; ++index) {
			if (equalValues(lhs[index], rhs[index])) {
				continue;
			}
			return lessValues(lhs[index], rhs[index]);
		}
		return lhs.size() < rhs.size();
	}
	if (isNumeric(left) && isNumeric(right)) {
		if (left.isFloat() || right.isFloat()) {
			return toDoubleNumeric(left) < toDoubleNumeric(right);
		}
		return toBigIntNumeric(left) < toBigIntNumeric(right);
	}
	return false;
}

bool equalValues(const Value &left, const Value &right) {
	if (left.isNone() || right.isNone()) {
		return left.isNone() && right.isNone();
	}
	if (left.isString() || right.isString()) {
		return left.isString() && right.isString() && left.asString() == right.asString();
	}
	if (left.isTuple() || right.isTuple()) {
		if (!left.isTuple() || !right.isTuple()) {
			return false;
		}
		const auto &lhs = left.asTuple()->elements;
		const auto &rhs = right.asTuple()->elements;
		if (lhs.size() != rhs.size()) {
			return false;
		}
		for (std::size_t index = 0; index < lhs.size(); ++index) {
			if (!equalValues(lhs[index], rhs[index])) {
				return false;
			}
		}
		return true;
	}
	if (left.isFunction() || right.isFunction()) {
		return left.isFunction() && right.isFunction() && left.asFunction() == right.asFunction();
	}
	if (isNumeric(left) && isNumeric(right)) {
		if (left.isFloat() || right.isFloat()) {
			return toDoubleNumeric(left) == toDoubleNumeric(right);
		}
		return toBigIntNumeric(left) == toBigIntNumeric(right);
	}
	return false;
}

bool compareValues(const Value &left, const Value &right, int tokenType) {
	switch (tokenType) {
	case Python3Parser::LESS_THAN:
		return lessValues(left, right);
	case Python3Parser::GREATER_THAN:
		return lessValues(right, left);
	case Python3Parser::LT_EQ:
		return !lessValues(right, left);
	case Python3Parser::GT_EQ:
		return !lessValues(left, right);
	case Python3Parser::EQUALS:
		return equalValues(left, right);
	case Python3Parser::NOT_EQ_2:
		return !equalValues(left, right);
	default:
		return false;
	}
}

std::string tupleString(const std::vector<Value> &values) {
	std::ostringstream stream;
	stream << '(';
	for (std::size_t index = 0; index < values.size(); ++index) {
		if (index > 0) {
			stream << ", ";
		}
		stream << reprString(values[index]);
	}
	if (values.size() == 1) {
		stream << ',';
	}
	stream << ')';
	return stream.str();
}

std::string displayString(const Value &value) {
	if (value.isNone()) {
		return "None";
	}
	if (value.isBool()) {
		return value.asBool() ? "True" : "False";
	}
	if (value.isInt()) {
		return bigIntToString(value.asInt());
	}
	if (value.isFloat()) {
		return formatFloat(value.asFloat());
	}
	if (value.isString()) {
		return value.asString();
	}
	if (value.isTuple()) {
		return tupleString(value.asTuple()->elements);
	}
	return "<function>";
}

std::string reprString(const Value &value) {
	if (value.isString()) {
		return "'" + value.asString() + "'";
	}
	return displayString(value);
}

Value anyToValue(const std::any &result) {
	if (const auto *value = std::any_cast<Value>(&result)) {
		return *value;
	}
	return Value::none();
}

std::vector<Value> unpackForAssignment(const Value &value) {
	if (value.isTuple()) {
		return value.asTuple()->elements;
	}
	return {value};
}

std::string extractName(Python3Parser::TestContext *ctx) {
	auto *orTest = ctx->or_test();
	if (orTest->and_test().size() != 1) {
		return "";
	}
	auto *andTest = orTest->and_test(0);
	if (andTest->not_test().size() != 1) {
		return "";
	}
	auto *notTest = andTest->not_test(0);
	if (notTest->comparison() == nullptr) {
		return "";
	}
	auto *comparison = notTest->comparison();
	if (comparison->arith_expr().size() != 1 || !comparison->comp_op().empty()) {
		return "";
	}
	auto *arith = comparison->arith_expr(0);
	if (arith->term().size() != 1 || !arith->addorsub_op().empty()) {
		return "";
	}
	auto *term = arith->term(0);
	if (term->factor().size() != 1 || !term->muldivmod_op().empty()) {
		return "";
	}
	auto *factor = term->factor(0);
	if (factor->atom_expr() == nullptr || factor->factor() != nullptr) {
		return "";
	}
	auto *atomExpr = factor->atom_expr();
	if (atomExpr->trailer() != nullptr) {
		return "";
	}
	auto *atom = atomExpr->atom();
	return atom->NAME() ? atom->NAME()->getText() : "";
}

struct EvalVisitor::Impl {
	std::shared_ptr<Frame> globalFrame = std::make_shared<Frame>();
	std::shared_ptr<Frame> currentFrame = globalFrame;

	Impl() {
		globalFrame->isGlobal = true;
		installBuiltins();
	}

	void installBuiltins() {
		bindGlobalFunction("print", BuiltinKind::Print);
		bindGlobalFunction("int", BuiltinKind::Int);
		bindGlobalFunction("float", BuiltinKind::Float);
		bindGlobalFunction("str", BuiltinKind::Str);
		bindGlobalFunction("bool", BuiltinKind::Bool);
	}

	void bindGlobalFunction(const std::string &name, BuiltinKind kind) {
		auto function = std::make_shared<Function>();
		function->kind = kind;
		function->name = name;
		function->closure = globalFrame;
		globalFrame->values[name] = Value::function(std::move(function));
	}

	Value lookup(const std::string &name) const {
		for (auto frame = currentFrame; frame != nullptr; frame = frame->parent) {
			auto found = frame->values.find(name);
			if (found != frame->values.end()) {
				return found->second;
			}
		}
		return Value::none();
	}

	void assign(const std::string &name, const Value &value) {
		if (currentFrame->isGlobal) {
			currentFrame->values[name] = value;
			return;
		}
		auto local = currentFrame->values.find(name);
		if (local != currentFrame->values.end()) {
			local->second = value;
			return;
		}
		auto global = globalFrame->values.find(name);
		if (global != globalFrame->values.end()) {
			global->second = value;
			return;
		}
		currentFrame->values[name] = value;
	}
};

Value buildTupleOrSingle(const std::vector<Value> &values, bool forceTuple) {
	if (!forceTuple && values.size() == 1) {
		return values.front();
	}
	return Value::tuple(values);
}

Value convertToIntValue(const Value &value) {
	if (value.isInt()) {
		return value;
	}
	if (value.isBool()) {
		return Value::integer(value.asBool() ? BigInt(1) : BigInt(0));
	}
	if (value.isFloat()) {
		return Value::integer(truncDoubleToBigInt(value.asFloat()));
	}
	if (value.isString()) {
		return Value::integer(parseBigInt(value.asString()));
	}
	return Value::integer(0);
}

Value convertToFloatValue(const Value &value) {
	if (value.isFloat()) {
		return value;
	}
	if (value.isBool()) {
		return Value::floating(value.asBool() ? 1.0 : 0.0);
	}
	if (value.isInt()) {
		return Value::floating(value.asInt().convert_to<double>());
	}
	if (value.isString()) {
		return Value::floating(std::stod(value.asString()));
	}
	return Value::floating(0.0);
}

Value convertToStringValue(const Value &value) {
	return Value::string(displayString(value));
}

Value convertToBoolValue(const Value &value) {
	return Value::boolean(truthy(value));
}

Value invokeFunction(EvalVisitor &visitor, EvalVisitor::Impl &impl, const Value &calleeValue,
                     const std::vector<CallArgument> &arguments) {
	const auto function = calleeValue.asFunction();
	switch (function->kind) {
	case BuiltinKind::Print: {
		for (std::size_t index = 0; index < arguments.size(); ++index) {
			if (index > 0) {
				std::cout << ' ';
			}
			std::cout << displayString(arguments[index].value);
		}
		std::cout << '\n';
		return Value::none();
	}
	case BuiltinKind::Int:
		return arguments.empty() ? Value::integer(0) : convertToIntValue(arguments.front().value);
	case BuiltinKind::Float:
		return arguments.empty() ? Value::floating(0.0) : convertToFloatValue(arguments.front().value);
	case BuiltinKind::Str:
		return arguments.empty() ? Value::string("") : convertToStringValue(arguments.front().value);
	case BuiltinKind::Bool:
		return arguments.empty() ? Value::boolean(false) : convertToBoolValue(arguments.front().value);
	case BuiltinKind::User:
		break;
	}

	std::vector<std::optional<Value>> bound(function->parameters.size());
	std::size_t positionalIndex = 0;
	for (const auto &argument : arguments) {
		if (!argument.keyword.has_value()) {
			while (positionalIndex < bound.size() && bound[positionalIndex].has_value()) {
				++positionalIndex;
			}
			if (positionalIndex < bound.size()) {
				bound[positionalIndex] = argument.value;
				++positionalIndex;
			}
			continue;
		}
		for (std::size_t index = 0; index < function->parameters.size(); ++index) {
			if (function->parameters[index].name == *argument.keyword) {
				bound[index] = argument.value;
				break;
			}
		}
	}
	for (std::size_t index = 0; index < function->parameters.size(); ++index) {
		if (!bound[index].has_value() && function->parameters[index].defaultValue.has_value()) {
			bound[index] = function->parameters[index].defaultValue;
		}
	}

	auto previousFrame = impl.currentFrame;
	auto callFrame = std::make_shared<Frame>();
	callFrame->parent = function->closure ? function->closure : impl.globalFrame;
	impl.currentFrame = callFrame;
	for (std::size_t index = 0; index < function->parameters.size(); ++index) {
		callFrame->values[function->parameters[index].name] = bound[index].value_or(Value::none());
	}

	try {
		visitor.visit(function->body);
		impl.currentFrame = previousFrame;
		return Value::none();
	} catch (const ReturnSignal &signal) {
		impl.currentFrame = previousFrame;
		return signal.value;
	} catch (...) {
		impl.currentFrame = previousFrame;
		throw;
	}
}

} // namespace

EvalVisitor::EvalVisitor() : impl_(std::make_unique<Impl>()) {}

EvalVisitor::~EvalVisitor() = default;

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
	for (auto *statement : ctx->stmt()) {
		visit(statement);
	}
	return Value::none();
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx) {
	auto function = std::make_shared<Function>();
	function->kind = BuiltinKind::User;
	function->name = ctx->NAME()->getText();
	function->body = ctx->suite();
	function->closure = impl_->currentFrame;
	function->parameters = std::any_cast<std::vector<Parameter>>(visit(ctx->parameters()));
	impl_->assign(function->name, Value::function(std::move(function)));
	return Value::none();
}

std::any EvalVisitor::visitParameters(Python3Parser::ParametersContext *ctx) {
	if (ctx->typedargslist() == nullptr) {
		return std::vector<Parameter>{};
	}
	return visit(ctx->typedargslist());
}

std::any EvalVisitor::visitTypedargslist(Python3Parser::TypedargslistContext *ctx) {
	std::vector<Parameter> parameters;
	parameters.reserve(ctx->tfpdef().size());

	const auto defaultCount = ctx->test().size();
	const auto firstDefault = ctx->tfpdef().size() - defaultCount;
	for (std::size_t index = 0; index < ctx->tfpdef().size(); ++index) {
		Parameter parameter{ctx->tfpdef(index)->NAME()->getText(), std::nullopt};
		if (index >= firstDefault) {
			parameter.defaultValue = anyToValue(visit(ctx->test(index - firstDefault)));
		}
		parameters.push_back(std::move(parameter));
	}
	return parameters;
}

std::any EvalVisitor::visitTfpdef(Python3Parser::TfpdefContext *ctx) {
	return ctx->NAME()->getText();
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
	if (ctx->simple_stmt() != nullptr) {
		return visit(ctx->simple_stmt());
	}
	return visit(ctx->compound_stmt());
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
	return visit(ctx->small_stmt());
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
	if (ctx->expr_stmt() != nullptr) {
		return visit(ctx->expr_stmt());
	}
	return visit(ctx->flow_stmt());
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
	if (ctx->augassign() != nullptr) {
		auto *targetList = ctx->testlist(0);
		const std::string name = extractName(targetList->test(0));
		const Value left = impl_->lookup(name);
		const Value right = anyToValue(visit(ctx->testlist(1)));
		const int op = ctx->augassign()->getStart()->getType();
		Value result = Value::none();
		if (op == Python3Parser::ADD_ASSIGN) {
			result = addValues(left, right);
		} else if (op == Python3Parser::SUB_ASSIGN) {
			result = subValues(left, right);
		} else if (op == Python3Parser::MULT_ASSIGN) {
			result = mulValues(left, right);
		} else if (op == Python3Parser::DIV_ASSIGN) {
			result = divValues(left, right);
		} else if (op == Python3Parser::IDIV_ASSIGN) {
			result = floorDivValues(left, right);
		} else if (op == Python3Parser::MOD_ASSIGN) {
			result = modValues(left, right);
		}
		impl_->assign(name, result);
		return result;
	}

	if (ctx->ASSIGN().empty()) {
		return visit(ctx->testlist(0));
	}

	const Value rightValue = anyToValue(visit(ctx->testlist(ctx->testlist().size() - 1)));
	for (std::size_t listIndex = 0; listIndex + 1 < ctx->testlist().size(); ++listIndex) {
		auto *targetList = ctx->testlist(listIndex);
		if (targetList->test().size() == 1 && targetList->COMMA().empty()) {
			impl_->assign(extractName(targetList->test(0)), rightValue);
			continue;
		}

		const std::vector<Value> values = unpackForAssignment(rightValue);
		for (std::size_t index = 0; index < targetList->test().size(); ++index) {
			impl_->assign(extractName(targetList->test(index)), values[index]);
		}
	}
	return rightValue;
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx) {
	return ctx->getText();
}

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) {
	if (ctx->break_stmt() != nullptr) {
		return visit(ctx->break_stmt());
	}
	if (ctx->continue_stmt() != nullptr) {
		return visit(ctx->continue_stmt());
	}
	return visit(ctx->return_stmt());
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
	(void)ctx;
	throw BreakSignal{};
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
	(void)ctx;
	throw ContinueSignal{};
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
	if (ctx->testlist() == nullptr) {
		throw ReturnSignal{Value::none()};
	}
	throw ReturnSignal{anyToValue(visit(ctx->testlist()))};
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) {
	if (ctx->if_stmt() != nullptr) {
		return visit(ctx->if_stmt());
	}
	if (ctx->while_stmt() != nullptr) {
		return visit(ctx->while_stmt());
	}
	return visit(ctx->funcdef());
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
	for (std::size_t index = 0; index < ctx->test().size(); ++index) {
		if (truthy(anyToValue(visit(ctx->test(index))))) {
			return visit(ctx->suite(index));
		}
	}
	if (ctx->ELSE() != nullptr) {
		return visit(ctx->suite(ctx->suite().size() - 1));
	}
	return Value::none();
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
	while (truthy(anyToValue(visit(ctx->test())))) {
		try {
			visit(ctx->suite());
		} catch (const ContinueSignal &) {
			continue;
		} catch (const BreakSignal &) {
			break;
		}
	}
	return Value::none();
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
	if (ctx->simple_stmt() != nullptr) {
		return visit(ctx->simple_stmt());
	}
	for (auto *statement : ctx->stmt()) {
		visit(statement);
	}
	return Value::none();
}

std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx) {
	return visit(ctx->or_test());
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
	bool result = truthy(anyToValue(visit(ctx->and_test(0))));
	for (std::size_t index = 1; index < ctx->and_test().size(); ++index) {
		if (result) {
			return Value::boolean(true);
		}
		result = truthy(anyToValue(visit(ctx->and_test(index))));
	}
	return Value::boolean(result);
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
	bool result = truthy(anyToValue(visit(ctx->not_test(0))));
	for (std::size_t index = 1; index < ctx->not_test().size(); ++index) {
		if (!result) {
			return Value::boolean(false);
		}
		result = truthy(anyToValue(visit(ctx->not_test(index))));
	}
	return Value::boolean(result);
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
	if (ctx->NOT() != nullptr) {
		return Value::boolean(!truthy(anyToValue(visit(ctx->not_test()))));
	}
	return visit(ctx->comparison());
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
	Value left = anyToValue(visit(ctx->arith_expr(0)));
	if (ctx->comp_op().empty()) {
		return left;
	}

	for (std::size_t index = 0; index < ctx->comp_op().size(); ++index) {
		Value right = anyToValue(visit(ctx->arith_expr(index + 1)));
		if (!compareValues(left, right, ctx->comp_op(index)->getStart()->getType())) {
			return Value::boolean(false);
		}
		left = right;
	}
	return Value::boolean(true);
}

std::any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx) {
	return ctx->getStart()->getType();
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
	Value value = anyToValue(visit(ctx->term(0)));
	for (std::size_t index = 0; index < ctx->addorsub_op().size(); ++index) {
		const int op = ctx->addorsub_op(index)->getStart()->getType();
		const Value right = anyToValue(visit(ctx->term(index + 1)));
		value = op == Python3Parser::ADD ? addValues(value, right) : subValues(value, right);
	}
	return value;
}

std::any EvalVisitor::visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) {
	return ctx->getStart()->getType();
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
	Value value = anyToValue(visit(ctx->factor(0)));
	for (std::size_t index = 0; index < ctx->muldivmod_op().size(); ++index) {
		const int op = ctx->muldivmod_op(index)->getStart()->getType();
		const Value right = anyToValue(visit(ctx->factor(index + 1)));
		if (op == Python3Parser::STAR) {
			value = mulValues(value, right);
		} else if (op == Python3Parser::DIV) {
			value = divValues(value, right);
		} else if (op == Python3Parser::IDIV) {
			value = floorDivValues(value, right);
		} else {
			value = modValues(value, right);
		}
	}
	return value;
}

std::any EvalVisitor::visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) {
	return ctx->getStart()->getType();
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
	if (ctx->atom_expr() != nullptr) {
		return visit(ctx->atom_expr());
	}
	const Value value = anyToValue(visit(ctx->factor()));
	if (ctx->ADD() != nullptr) {
		return plusValue(value);
	}
	return negateValue(value);
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
	const Value atomValue = anyToValue(visit(ctx->atom()));
	if (ctx->trailer() == nullptr) {
		return atomValue;
	}
	const auto arguments = std::any_cast<std::vector<CallArgument>>(visit(ctx->trailer()));
	return invokeFunction(*this, *impl_, atomValue, arguments);
}

std::any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx) {
	if (ctx->arglist() == nullptr) {
		return std::vector<CallArgument>{};
	}
	return visit(ctx->arglist());
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
	if (ctx->NAME() != nullptr) {
		return impl_->lookup(ctx->NAME()->getText());
	}
	if (ctx->NUMBER() != nullptr) {
		const std::string text = ctx->NUMBER()->getText();
		if (text.find('.') != std::string::npos) {
			return Value::floating(std::stod(text));
		}
		return Value::integer(parseBigInt(text));
	}
	if (!ctx->STRING().empty()) {
		std::string result;
		for (auto *token : ctx->STRING()) {
			result += unquoteLiteral(token->getText());
		}
		return Value::string(std::move(result));
	}
	if (ctx->NONE() != nullptr) {
		return Value::none();
	}
	if (ctx->TRUE() != nullptr) {
		return Value::boolean(true);
	}
	if (ctx->FALSE() != nullptr) {
		return Value::boolean(false);
	}
	if (ctx->format_string() != nullptr) {
		return visit(ctx->format_string());
	}
	return visit(ctx->test());
}

std::any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx) {
	std::string result;
	for (auto *child : ctx->children) {
		if (auto *terminal = dynamic_cast<antlr4::tree::TerminalNode *>(child)) {
			const int type = terminal->getSymbol()->getType();
			if (type == Python3Parser::FORMAT_STRING_LITERAL) {
				result += decodeFormatLiteral(terminal->getText());
			}
			continue;
		}
		if (auto *testList = dynamic_cast<Python3Parser::TestlistContext *>(child)) {
			result += displayString(anyToValue(visit(testList)));
		}
	}
	return Value::string(std::move(result));
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
	std::vector<Value> values;
	values.reserve(ctx->test().size());
	for (auto *test : ctx->test()) {
		values.push_back(anyToValue(visit(test)));
	}
	return buildTupleOrSingle(values, !ctx->COMMA().empty());
}

std::any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx) {
	std::vector<CallArgument> arguments;
	arguments.reserve(ctx->argument().size());
	for (auto *argument : ctx->argument()) {
		arguments.push_back(std::any_cast<CallArgument>(visit(argument)));
	}
	return arguments;
}

std::any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx) {
	if (ctx->ASSIGN() != nullptr) {
		return CallArgument{extractName(ctx->test(0)), anyToValue(visit(ctx->test(1)))};
	}
	return CallArgument{std::nullopt, anyToValue(visit(ctx->test(0)))};
}
