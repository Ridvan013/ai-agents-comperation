/**
 * @file evaluation.cpp
 * @brief Expression evaluation implementation for the Scheme interpreter
 */

#include "value.hpp"
#include "expr.hpp"
#include "RE.hpp"
#include "syntax.hpp"

#include <climits>
#include <iostream>
#include <map>
#include <string>
#include <vector>

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

namespace {

const std::string kFrameMarker(1, '\x1F');
const std::string kVariadicPrimitiveMarker(1, '\x1E');

bool isTruthy(const Value &value) {
    return !(value->v_type == V_BOOL && !dynamic_cast<Boolean *>(value.get())->b);
}

bool isFrameBinding(const std::string &name) {
    return name == kFrameMarker;
}

bool isVariadicPrimitiveProcedure(const Procedure *proc) {
    return proc->parameters.size() == 1 && proc->parameters[0] == kVariadicPrimitiveMarker;
}

Assoc makeFrame(const Assoc &parent) {
    Assoc copy = parent;
    return extend(kFrameMarker, VoidV(), copy);
}

bool existsInCurrentFrame(const std::string &name, Assoc &env) {
    for (Assoc current = env; current.get() != nullptr; current = current->next) {
        if (isFrameBinding(current->x)) {
            break;
        }
        if (current->x == name) {
            return true;
        }
    }
    return false;
}

void defineInCurrentFrame(const std::string &name, const Value &value, Assoc &env) {
    if (existsInCurrentFrame(name, env)) {
        for (Assoc current = env; current.get() != nullptr; current = current->next) {
            if (isFrameBinding(current->x)) {
                break;
            }
            if (current->x == name) {
                current->v = value;
                return;
            }
        }
    }
    env = extend(name, value, env);
}

struct Numeric {
    int num;
    int den;
};

Numeric asNumeric(const Value &value) {
    if (value->v_type == V_INT) {
        return {dynamic_cast<Integer *>(value.get())->n, 1};
    }
    if (value->v_type == V_RATIONAL) {
        Rational *rational = dynamic_cast<Rational *>(value.get());
        return {rational->numerator, rational->denominator};
    }
    throw RuntimeError("Wrong typename");
}

Value makeNumericValue(int numerator, int denominator) {
    if (denominator == 0) {
        throw RuntimeError("Division by zero");
    }
    Value rational = RationalV(numerator, denominator);
    Rational *ptr = dynamic_cast<Rational *>(rational.get());
    if (ptr->denominator == 1) {
        return IntegerV(ptr->numerator);
    }
    return rational;
}

Value quoteToValue(const Syntax &syntax) {
    if (auto number = dynamic_cast<Number *>(syntax.get())) {
        return IntegerV(number->n);
    }
    if (auto rational = dynamic_cast<RationalSyntax *>(syntax.get())) {
        return RationalV(rational->numerator, rational->denominator);
    }
    if (dynamic_cast<TrueSyntax *>(syntax.get()) != nullptr) {
        return BooleanV(true);
    }
    if (dynamic_cast<FalseSyntax *>(syntax.get()) != nullptr) {
        return BooleanV(false);
    }
    if (auto symbol = dynamic_cast<SymbolSyntax *>(syntax.get())) {
        return SymbolV(symbol->s);
    }
    if (auto str = dynamic_cast<StringSyntax *>(syntax.get())) {
        return StringV(str->s);
    }

    auto list = dynamic_cast<List *>(syntax.get());
    if (list == nullptr) {
        throw RuntimeError("Unsupported quote syntax");
    }
    if (list->stxs.empty()) {
        return NullV();
    }

    int dot_index = -1;
    for (size_t i = 0; i < list->stxs.size(); ++i) {
        auto symbol = dynamic_cast<SymbolSyntax *>(list->stxs[i].get());
        if (symbol != nullptr && symbol->s == ".") {
            if (dot_index != -1) {
                throw RuntimeError("Invalid dotted list");
            }
            dot_index = static_cast<int>(i);
        }
    }

    if (dot_index != -1 && (dot_index == 0 || dot_index != static_cast<int>(list->stxs.size()) - 2)) {
        throw RuntimeError("Invalid dotted list");
    }

    Value tail = NullV();
    int last_index = static_cast<int>(list->stxs.size());
    if (dot_index != -1) {
        tail = quoteToValue(list->stxs.back());
        last_index = dot_index;
    }

    for (int i = last_index - 1; i >= 0; --i) {
        tail = PairV(quoteToValue(list->stxs[static_cast<size_t>(i)]), tail);
    }
    return tail;
}

Value makePrimitiveProcedure(const std::string &name) {
    Assoc primitive_env = empty();

    if (name == "+") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new PlusVar({})), primitive_env);
    if (name == "-") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new MinusVar({})), primitive_env);
    if (name == "*") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new MultVar({})), primitive_env);
    if (name == "/") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new DivVar({})), primitive_env);
    if (name == "<") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new LessVar({})), primitive_env);
    if (name == "<=") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new LessEqVar({})), primitive_env);
    if (name == "=") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new EqualVar({})), primitive_env);
    if (name == ">=") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new GreaterEqVar({})), primitive_env);
    if (name == ">") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new GreaterVar({})), primitive_env);
    if (name == "list") return ProcedureV({kVariadicPrimitiveMarker}, Expr(new ListFunc({})), primitive_env);

    if (name == "void") return ProcedureV({}, Expr(new MakeVoid()), primitive_env);
    if (name == "exit") return ProcedureV({}, Expr(new Exit()), primitive_env);
    if (name == "not") return ProcedureV({"arg"}, Expr(new Not(Expr(new Var("arg")))), primitive_env);
    if (name == "cons") return ProcedureV({"a", "b"}, Expr(new Cons(Expr(new Var("a")), Expr(new Var("b")))), primitive_env);
    if (name == "car") return ProcedureV({"arg"}, Expr(new Car(Expr(new Var("arg")))), primitive_env);
    if (name == "cdr") return ProcedureV({"arg"}, Expr(new Cdr(Expr(new Var("arg")))), primitive_env);
    if (name == "set-car!") return ProcedureV({"pair", "value"}, Expr(new SetCar(Expr(new Var("pair")), Expr(new Var("value")))), primitive_env);
    if (name == "set-cdr!") return ProcedureV({"pair", "value"}, Expr(new SetCdr(Expr(new Var("pair")), Expr(new Var("value")))), primitive_env);
    if (name == "modulo") return ProcedureV({"a", "b"}, Expr(new Modulo(Expr(new Var("a")), Expr(new Var("b")))), primitive_env);
    if (name == "expt") return ProcedureV({"a", "b"}, Expr(new Expt(Expr(new Var("a")), Expr(new Var("b")))), primitive_env);
    if (name == "eq?") return ProcedureV({"a", "b"}, Expr(new IsEq(Expr(new Var("a")), Expr(new Var("b")))), primitive_env);
    if (name == "boolean?") return ProcedureV({"arg"}, Expr(new IsBoolean(Expr(new Var("arg")))), primitive_env);
    if (name == "number?") return ProcedureV({"arg"}, Expr(new IsFixnum(Expr(new Var("arg")))), primitive_env);
    if (name == "null?") return ProcedureV({"arg"}, Expr(new IsNull(Expr(new Var("arg")))), primitive_env);
    if (name == "pair?") return ProcedureV({"arg"}, Expr(new IsPair(Expr(new Var("arg")))), primitive_env);
    if (name == "procedure?") return ProcedureV({"arg"}, Expr(new IsProcedure(Expr(new Var("arg")))), primitive_env);
    if (name == "symbol?") return ProcedureV({"arg"}, Expr(new IsSymbol(Expr(new Var("arg")))), primitive_env);
    if (name == "list?") return ProcedureV({"arg"}, Expr(new IsList(Expr(new Var("arg")))), primitive_env);
    if (name == "string?") return ProcedureV({"arg"}, Expr(new IsString(Expr(new Var("arg")))), primitive_env);
    if (name == "display") return ProcedureV({"arg"}, Expr(new Display(Expr(new Var("arg")))), primitive_env);

    throw RuntimeError("Unknown primitive");
}

bool isValidIdentifier(const std::string &name) {
    if (name.empty()) {
        return false;
    }

    char first = name[0];
    if ((first >= '0' && first <= '9') || first == '.' || first == '@') {
        return false;
    }

    if (name.find_first_of("#'\"`") != std::string::npos) {
        return false;
    }

    for (char ch : name) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

bool isValidDefinitionName(const std::string &name) {
    return isValidIdentifier(name) &&
           primitives.count(name) == 0 &&
           reserved_words.count(name) == 0;
}

} // namespace

Value Fixnum::eval(Assoc &e) {
    return IntegerV(n);
}

Value RationalNum::eval(Assoc &e) {
    return RationalV(numerator, denominator);
}

Value StringExpr::eval(Assoc &e) {
    return StringV(s);
}

Value True::eval(Assoc &e) {
    return BooleanV(true);
}

Value False::eval(Assoc &e) {
    return BooleanV(false);
}

Value MakeVoid::eval(Assoc &e) {
    return VoidV();
}

Value Exit::eval(Assoc &e) {
    return TerminateV();
}

Value Unary::eval(Assoc &e) {
    return evalRator(rand->eval(e));
}

Value Binary::eval(Assoc &e) {
    return evalRator(rand1->eval(e), rand2->eval(e));
}

Value Variadic::eval(Assoc &e) {
    std::vector<Value> values;
    for (const Expr &rand : rands) {
        values.push_back(rand->eval(e));
    }
    return evalRator(values);
}

Value Var::eval(Assoc &e) {
    if (!isValidIdentifier(x) && primitives.count(x) == 0) {
        throw RuntimeError("Invalid identifier");
    }

    Value matched_value = find(x, e);
    if (matched_value.get() != nullptr) {
        return matched_value;
    }

    if (primitives.count(x) != 0) {
        if (x == "and" || x == "or") {
            throw RuntimeError("Short-circuit primitives are not first-class");
        }
        return makePrimitiveProcedure(x);
    }

    throw RuntimeError("Undefined variable");
}

Value Plus::evalRator(const Value &rand1, const Value &rand2) {
    Numeric lhs = asNumeric(rand1);
    Numeric rhs = asNumeric(rand2);
    return makeNumericValue(lhs.num * rhs.den + rhs.num * lhs.den, lhs.den * rhs.den);
}

Value Minus::evalRator(const Value &rand1, const Value &rand2) {
    Numeric lhs = asNumeric(rand1);
    Numeric rhs = asNumeric(rand2);
    return makeNumericValue(lhs.num * rhs.den - rhs.num * lhs.den, lhs.den * rhs.den);
}

Value Mult::evalRator(const Value &rand1, const Value &rand2) {
    Numeric lhs = asNumeric(rand1);
    Numeric rhs = asNumeric(rand2);
    return makeNumericValue(lhs.num * rhs.num, lhs.den * rhs.den);
}

Value Div::evalRator(const Value &rand1, const Value &rand2) {
    Numeric lhs = asNumeric(rand1);
    Numeric rhs = asNumeric(rand2);
    if (rhs.num == 0) {
        throw RuntimeError("Division by zero");
    }
    return makeNumericValue(lhs.num * rhs.den, lhs.den * rhs.num);
}

Value Modulo::evalRator(const Value &rand1, const Value &rand2) {
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int dividend = dynamic_cast<Integer *>(rand1.get())->n;
        int divisor = dynamic_cast<Integer *>(rand2.get())->n;
        if (divisor == 0) {
            throw RuntimeError("Division by zero");
        }
        return IntegerV(dividend % divisor);
    }
    throw RuntimeError("modulo is only defined for integers");
}

Value PlusVar::evalRator(const std::vector<Value> &args) {
    Value result = IntegerV(0);
    for (const Value &arg : args) {
        result = Plus(Expr(new Fixnum(0)), Expr(new Fixnum(0))).evalRator(result, arg);
    }
    return result;
}

Value MinusVar::evalRator(const std::vector<Value> &args) {
    if (args.empty()) {
        throw RuntimeError("Wrong number of arguments");
    }
    if (args.size() == 1) {
        return Minus(Expr(new Fixnum(0)), Expr(new Fixnum(0))).evalRator(IntegerV(0), args[0]);
    }

    Value result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = Minus(Expr(new Fixnum(0)), Expr(new Fixnum(0))).evalRator(result, args[i]);
    }
    return result;
}

Value MultVar::evalRator(const std::vector<Value> &args) {
    Value result = IntegerV(1);
    for (const Value &arg : args) {
        result = Mult(Expr(new Fixnum(0)), Expr(new Fixnum(0))).evalRator(result, arg);
    }
    return result;
}

Value DivVar::evalRator(const std::vector<Value> &args) {
    if (args.empty()) {
        throw RuntimeError("Wrong number of arguments");
    }
    if (args.size() == 1) {
        return Div(Expr(new Fixnum(0)), Expr(new Fixnum(0))).evalRator(IntegerV(1), args[0]);
    }

    Value result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = Div(Expr(new Fixnum(0)), Expr(new Fixnum(0))).evalRator(result, args[i]);
    }
    return result;
}

Value Expt::evalRator(const Value &rand1, const Value &rand2) {
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int base = dynamic_cast<Integer *>(rand1.get())->n;
        int exponent = dynamic_cast<Integer *>(rand2.get())->n;

        if (exponent < 0) {
            throw RuntimeError("Negative exponent not supported for integers");
        }
        if (base == 0 && exponent == 0) {
            throw RuntimeError("0^0 is undefined");
        }

        long long result = 1;
        long long current = base;
        int exp = exponent;
        while (exp > 0) {
            if (exp & 1) {
                result *= current;
                if (result > INT_MAX || result < INT_MIN) {
                    throw RuntimeError("Integer overflow in expt");
                }
            }
            exp >>= 1;
            if (exp > 0) {
                current *= current;
                if (current > INT_MAX || current < INT_MIN) {
                    throw RuntimeError("Integer overflow in expt");
                }
            }
        }
        return IntegerV(static_cast<int>(result));
    }
    throw RuntimeError("Wrong typename");
}

int compareNumericValues(const Value &v1, const Value &v2) {
    Numeric lhs = asNumeric(v1);
    Numeric rhs = asNumeric(v2);

    long long left = static_cast<long long>(lhs.num) * rhs.den;
    long long right = static_cast<long long>(rhs.num) * lhs.den;
    if (left < right) {
        return -1;
    }
    if (left > right) {
        return 1;
    }
    return 0;
}

Value Less::evalRator(const Value &rand1, const Value &rand2) {
    return BooleanV(compareNumericValues(rand1, rand2) < 0);
}

Value LessEq::evalRator(const Value &rand1, const Value &rand2) {
    return BooleanV(compareNumericValues(rand1, rand2) <= 0);
}

Value Equal::evalRator(const Value &rand1, const Value &rand2) {
    return BooleanV(compareNumericValues(rand1, rand2) == 0);
}

Value GreaterEq::evalRator(const Value &rand1, const Value &rand2) {
    return BooleanV(compareNumericValues(rand1, rand2) >= 0);
}

Value Greater::evalRator(const Value &rand1, const Value &rand2) {
    return BooleanV(compareNumericValues(rand1, rand2) > 0);
}

Value LessVar::evalRator(const std::vector<Value> &args) {
    if (args.size() < 2) {
        throw RuntimeError("Wrong number of arguments");
    }
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i - 1], args[i]) >= 0) {
            return BooleanV(false);
        }
    }
    return BooleanV(true);
}

Value LessEqVar::evalRator(const std::vector<Value> &args) {
    if (args.size() < 2) {
        throw RuntimeError("Wrong number of arguments");
    }
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i - 1], args[i]) > 0) {
            return BooleanV(false);
        }
    }
    return BooleanV(true);
}

Value EqualVar::evalRator(const std::vector<Value> &args) {
    if (args.size() < 2) {
        throw RuntimeError("Wrong number of arguments");
    }
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i - 1], args[i]) != 0) {
            return BooleanV(false);
        }
    }
    return BooleanV(true);
}

Value GreaterEqVar::evalRator(const std::vector<Value> &args) {
    if (args.size() < 2) {
        throw RuntimeError("Wrong number of arguments");
    }
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i - 1], args[i]) < 0) {
            return BooleanV(false);
        }
    }
    return BooleanV(true);
}

Value GreaterVar::evalRator(const std::vector<Value> &args) {
    if (args.size() < 2) {
        throw RuntimeError("Wrong number of arguments");
    }
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i - 1], args[i]) <= 0) {
            return BooleanV(false);
        }
    }
    return BooleanV(true);
}

Value Cons::evalRator(const Value &rand1, const Value &rand2) {
    return PairV(rand1, rand2);
}

Value ListFunc::evalRator(const std::vector<Value> &args) {
    Value result = NullV();
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        result = PairV(*it, result);
    }
    return result;
}

Value IsList::evalRator(const Value &rand) {
    Value current = rand;
    while (current->v_type == V_PAIR) {
        current = dynamic_cast<Pair *>(current.get())->cdr;
    }
    return BooleanV(current->v_type == V_NULL);
}

Value Car::evalRator(const Value &rand) {
    if (rand->v_type != V_PAIR) {
        throw RuntimeError("car expects a pair");
    }
    return dynamic_cast<Pair *>(rand.get())->car;
}

Value Cdr::evalRator(const Value &rand) {
    if (rand->v_type != V_PAIR) {
        throw RuntimeError("cdr expects a pair");
    }
    return dynamic_cast<Pair *>(rand.get())->cdr;
}

Value SetCar::evalRator(const Value &rand1, const Value &rand2) {
    if (rand1->v_type != V_PAIR) {
        throw RuntimeError("set-car! expects a pair");
    }
    dynamic_cast<Pair *>(rand1.get())->car = rand2;
    return VoidV();
}

Value SetCdr::evalRator(const Value &rand1, const Value &rand2) {
    if (rand1->v_type != V_PAIR) {
        throw RuntimeError("set-cdr! expects a pair");
    }
    dynamic_cast<Pair *>(rand1.get())->cdr = rand2;
    return VoidV();
}

Value IsEq::evalRator(const Value &rand1, const Value &rand2) {
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        return BooleanV(dynamic_cast<Integer *>(rand1.get())->n == dynamic_cast<Integer *>(rand2.get())->n);
    }
    if (rand1->v_type == V_BOOL && rand2->v_type == V_BOOL) {
        return BooleanV(dynamic_cast<Boolean *>(rand1.get())->b == dynamic_cast<Boolean *>(rand2.get())->b);
    }
    if (rand1->v_type == V_SYM && rand2->v_type == V_SYM) {
        return BooleanV(dynamic_cast<Symbol *>(rand1.get())->s == dynamic_cast<Symbol *>(rand2.get())->s);
    }
    if ((rand1->v_type == V_NULL && rand2->v_type == V_NULL) ||
        (rand1->v_type == V_VOID && rand2->v_type == V_VOID)) {
        return BooleanV(true);
    }
    return BooleanV(rand1.get() == rand2.get());
}

Value IsBoolean::evalRator(const Value &rand) {
    return BooleanV(rand->v_type == V_BOOL);
}

Value IsFixnum::evalRator(const Value &rand) {
    return BooleanV(rand->v_type == V_INT);
}

Value IsNull::evalRator(const Value &rand) {
    return BooleanV(rand->v_type == V_NULL);
}

Value IsPair::evalRator(const Value &rand) {
    return BooleanV(rand->v_type == V_PAIR);
}

Value IsProcedure::evalRator(const Value &rand) {
    return BooleanV(rand->v_type == V_PROC);
}

Value IsSymbol::evalRator(const Value &rand) {
    return BooleanV(rand->v_type == V_SYM);
}

Value IsString::evalRator(const Value &rand) {
    return BooleanV(rand->v_type == V_STRING);
}

Value Begin::eval(Assoc &e) {
    Value result = VoidV();
    for (const Expr &expr : es) {
        result = expr->eval(e);
    }
    return result;
}

Value Quote::eval(Assoc &e) {
    return quoteToValue(s);
}

Value AndVar::eval(Assoc &e) {
    Value result = BooleanV(true);
    for (const Expr &expr : rands) {
        result = expr->eval(e);
        if (!isTruthy(result)) {
            return BooleanV(false);
        }
    }
    return result;
}

Value OrVar::eval(Assoc &e) {
    if (rands.empty()) {
        return BooleanV(false);
    }

    for (const Expr &expr : rands) {
        Value result = expr->eval(e);
        if (isTruthy(result)) {
            return result;
        }
        if (&expr == &rands.back()) {
            return result;
        }
    }
    return BooleanV(false);
}

Value Not::evalRator(const Value &rand) {
    return BooleanV(!isTruthy(rand));
}

Value If::eval(Assoc &e) {
    Value cond_value = cond->eval(e);
    if (isTruthy(cond_value)) {
        return conseq->eval(e);
    }
    return alter->eval(e);
}

Value Cond::eval(Assoc &env) {
    for (const auto &clause : clauses) {
        if (clause.empty()) {
            throw RuntimeError("Invalid cond clause");
        }

        bool is_else = false;
        Value predicate_value = BooleanV(true);
        if (auto var = dynamic_cast<Var *>(clause[0].get())) {
            is_else = (var->x == "else");
        }
        if (!is_else) {
            predicate_value = clause[0]->eval(env);
        }

        if (is_else || isTruthy(predicate_value)) {
            if (clause.size() == 1) {
                return predicate_value;
            }

            Value result = VoidV();
            for (size_t i = 1; i < clause.size(); ++i) {
                result = clause[i]->eval(env);
            }
            return result;
        }
    }
    return VoidV();
}

Value Lambda::eval(Assoc &env) {
    for (const std::string &name : x) {
        if (!isValidIdentifier(name)) {
            throw RuntimeError("Invalid parameter name");
        }
    }
    return ProcedureV(x, e, env);
}

Value Apply::eval(Assoc &e) {
    Value rator_value = rator->eval(e);
    if (rator_value->v_type != V_PROC) {
        throw RuntimeError("Attempt to apply a non-procedure");
    }

    Procedure *clos_ptr = dynamic_cast<Procedure *>(rator_value.get());

    if (isVariadicPrimitiveProcedure(clos_ptr)) {
        auto variadic = dynamic_cast<Variadic *>(clos_ptr->e.get());
        if (variadic == nullptr) {
            throw RuntimeError("Invalid primitive procedure");
        }

        std::vector<Value> args;
        for (const Expr &arg_expr : rand) {
            args.push_back(arg_expr->eval(e));
        }
        return variadic->evalRator(args);
    }

    if (rand.size() != clos_ptr->parameters.size()) {
        throw RuntimeError("Wrong number of arguments");
    }

    Assoc param_env = makeFrame(clos_ptr->env);
    for (size_t i = 0; i < rand.size(); ++i) {
        Value arg_value = rand[i]->eval(e);
        param_env = extend(clos_ptr->parameters[i], arg_value, param_env);
    }

    return clos_ptr->e->eval(param_env);
}

Value Define::eval(Assoc &env) {
    if (!isValidDefinitionName(var)) {
        throw RuntimeError("Invalid define target");
    }

    bool needs_placeholder = dynamic_cast<Lambda *>(e.get()) != nullptr;
    if (needs_placeholder) {
        defineInCurrentFrame(var, VoidV(), env);
        Value result = e->eval(env);
        defineInCurrentFrame(var, result, env);
        return VoidV();
    }

    Value result = e->eval(env);
    defineInCurrentFrame(var, result, env);
    return VoidV();
}

Value Let::eval(Assoc &env) {
    std::vector<Value> values;
    values.reserve(bind.size());
    for (const auto &entry : bind) {
        if (!isValidIdentifier(entry.first)) {
            throw RuntimeError("Invalid binding name");
        }
        values.push_back(entry.second->eval(env));
    }

    Assoc local_env = makeFrame(env);
    for (size_t i = 0; i < bind.size(); ++i) {
        local_env = extend(bind[i].first, values[i], local_env);
    }
    return body->eval(local_env);
}

Value Letrec::eval(Assoc &env) {
    Assoc local_env = makeFrame(env);
    for (const auto &entry : bind) {
        if (!isValidIdentifier(entry.first)) {
            throw RuntimeError("Invalid binding name");
        }
        local_env = extend(entry.first, VoidV(), local_env);
    }

    for (const auto &entry : bind) {
        modify(entry.first, entry.second->eval(local_env), local_env);
    }
    return body->eval(local_env);
}

Value Set::eval(Assoc &env) {
    if (!isValidIdentifier(var)) {
        throw RuntimeError("Invalid set! target");
    }

    Value existing = find(var, env);
    if (existing.get() == nullptr) {
        throw RuntimeError("Undefined variable");
    }

    Value new_value = e->eval(env);
    modify(var, new_value, env);
    return VoidV();
}

Value Display::evalRator(const Value &rand) {
    if (rand->v_type == V_STRING) {
        String *str_ptr = dynamic_cast<String *>(rand.get());
        std::cout << str_ptr->s;
    } else {
        rand->show(std::cout);
    }
    return VoidV();
}
