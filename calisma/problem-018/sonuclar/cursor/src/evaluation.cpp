/**
 * @file evaluation.cpp
 * @brief Expression evaluation implementation for the Scheme interpreter
 * @author luke36
 *
 * This file implements evaluation methods for all expression types in the Scheme
 * interpreter. Functions are organized according to ExprType enumeration order
 * from Def.hpp for consistency and maintainability.
 */

#include "value.hpp"
#include "expr.hpp"
#include "RE.hpp"
#include "syntax.hpp"
#include <cstring>
#include <vector>
#include <map>
#include <climits>

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

// Sentinel parameter name identifying a first-class primitive procedure that
// accepts a variable number of arguments (e.g. +, -, list, and, or). A user
// cannot type this name because variables may not begin with '@'.
static const std::string VARIADIC_SENTINEL = "@__args__";

// ============================================================================
// Numeric helpers (shared by arithmetic and comparison operators)
// ============================================================================

static long long gcdll(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

// Extract a value as a fraction num/den. Throws for non-numeric values.
static void asFraction(const Value &v, long long &num, long long &den) {
    if (v->v_type == V_INT) {
        num = dynamic_cast<Integer *>(v.get())->n;
        den = 1;
    } else if (v->v_type == V_RATIONAL) {
        Rational *r = dynamic_cast<Rational *>(v.get());
        num = r->numerator;
        den = r->denominator;
    } else {
        throw RuntimeError("Wrong typename: expected a number");
    }
}

// Build the simplest numeric value for num/den. Returns Integer when the
// denominator reduces to 1, otherwise a Rational.
static Value makeNumber(long long num, long long den) {
    if (den == 0)
        throw RuntimeError("Division by zero");
    if (den < 0) {
        num = -num;
        den = -den;
    }
    long long g = gcdll(num, den);
    if (g == 0) g = 1;
    num /= g;
    den /= g;
    if (den == 1)
        return IntegerV((int)num);
    return RationalV((int)num, (int)den);
}

Value Fixnum::eval(Assoc &e) { // evaluation of a fixnum
    return IntegerV(n);
}

Value RationalNum::eval(Assoc &e) { // evaluation of a rational number
    if (denominator == 1)
        return IntegerV(numerator);
    return RationalV(numerator, denominator);
}

Value StringExpr::eval(Assoc &e) { // evaluation of a string
    return StringV(s);
}

Value True::eval(Assoc &e) { // evaluation of #t
    return BooleanV(true);
}

Value False::eval(Assoc &e) { // evaluation of #f
    return BooleanV(false);
}

Value MakeVoid::eval(Assoc &e) { // (void)
    return VoidV();
}

Value Exit::eval(Assoc &e) { // (exit)
    return TerminateV();
}

Value Unary::eval(Assoc &e) { // evaluation of single-operator primitive
    return evalRator(rand->eval(e));
}

Value Binary::eval(Assoc &e) { // evaluation of two-operators primitive
    return evalRator(rand1->eval(e), rand2->eval(e));
}

Value Variadic::eval(Assoc &e) { // evaluation of multi-operator primitive
    std::vector<Value> args;
    args.reserve(rands.size());
    for (auto &r : rands)
        args.push_back(r->eval(e));
    return evalRator(args);
}

// Helper: build a first-class procedure for a primitive that is used as a value.
static Value makePrimitiveProcedure(ExprType t, Assoc &env) {
    switch (t) {
        // Variadic primitives use a sentinel parameter recognised by Apply.
        case E_PLUS:  return ProcedureV({VARIADIC_SENTINEL}, Expr(new PlusVar({})), env);
        case E_MINUS: return ProcedureV({VARIADIC_SENTINEL}, Expr(new MinusVar({})), env);
        case E_MUL:   return ProcedureV({VARIADIC_SENTINEL}, Expr(new MultVar({})), env);
        case E_DIV:   return ProcedureV({VARIADIC_SENTINEL}, Expr(new DivVar({})), env);
        case E_LT:    return ProcedureV({VARIADIC_SENTINEL}, Expr(new LessVar({})), env);
        case E_LE:    return ProcedureV({VARIADIC_SENTINEL}, Expr(new LessEqVar({})), env);
        case E_EQ:    return ProcedureV({VARIADIC_SENTINEL}, Expr(new EqualVar({})), env);
        case E_GE:    return ProcedureV({VARIADIC_SENTINEL}, Expr(new GreaterEqVar({})), env);
        case E_GT:    return ProcedureV({VARIADIC_SENTINEL}, Expr(new GreaterVar({})), env);
        case E_LIST:  return ProcedureV({VARIADIC_SENTINEL}, Expr(new ListFunc({})), env);
        case E_AND:   return ProcedureV({VARIADIC_SENTINEL}, Expr(new AndVar({})), env);
        case E_OR:    return ProcedureV({VARIADIC_SENTINEL}, Expr(new OrVar({})), env);

        // Fixed-arity primitives use ordinary parameter names.
        case E_MODULO:
            return ProcedureV({"a", "b"}, Expr(new Modulo(Expr(new Var("a")), Expr(new Var("b")))), env);
        case E_EXPT:
            return ProcedureV({"a", "b"}, Expr(new Expt(Expr(new Var("a")), Expr(new Var("b")))), env);
        case E_CONS:
            return ProcedureV({"a", "b"}, Expr(new Cons(Expr(new Var("a")), Expr(new Var("b")))), env);
        case E_CAR:
            return ProcedureV({"a"}, Expr(new Car(Expr(new Var("a")))), env);
        case E_CDR:
            return ProcedureV({"a"}, Expr(new Cdr(Expr(new Var("a")))), env);
        case E_SETCAR:
            return ProcedureV({"a", "b"}, Expr(new SetCar(Expr(new Var("a")), Expr(new Var("b")))), env);
        case E_SETCDR:
            return ProcedureV({"a", "b"}, Expr(new SetCdr(Expr(new Var("a")), Expr(new Var("b")))), env);
        case E_NOT:
            return ProcedureV({"a"}, Expr(new Not(Expr(new Var("a")))), env);
        case E_EQQ:
            return ProcedureV({"a", "b"}, Expr(new IsEq(Expr(new Var("a")), Expr(new Var("b")))), env);
        case E_BOOLQ:
            return ProcedureV({"a"}, Expr(new IsBoolean(Expr(new Var("a")))), env);
        case E_INTQ:
            return ProcedureV({"a"}, Expr(new IsFixnum(Expr(new Var("a")))), env);
        case E_NULLQ:
            return ProcedureV({"a"}, Expr(new IsNull(Expr(new Var("a")))), env);
        case E_PAIRQ:
            return ProcedureV({"a"}, Expr(new IsPair(Expr(new Var("a")))), env);
        case E_PROCQ:
            return ProcedureV({"a"}, Expr(new IsProcedure(Expr(new Var("a")))), env);
        case E_SYMBOLQ:
            return ProcedureV({"a"}, Expr(new IsSymbol(Expr(new Var("a")))), env);
        case E_LISTQ:
            return ProcedureV({"a"}, Expr(new IsList(Expr(new Var("a")))), env);
        case E_STRINGQ:
            return ProcedureV({"a"}, Expr(new IsString(Expr(new Var("a")))), env);
        case E_DISPLAY:
            return ProcedureV({"a"}, Expr(new Display(Expr(new Var("a")))), env);
        case E_VOID:
            return ProcedureV({}, Expr(new MakeVoid()), env);
        case E_EXIT:
            return ProcedureV({}, Expr(new Exit()), env);
        default:
            return Value(nullptr);
    }
}

Value Var::eval(Assoc &e) { // evaluation of variable
    // Look up the binding manually so we can distinguish "not bound" from
    // "bound but uninitialised" (used by letrec to reject premature use).
    for (Assoc i = e; i.get() != nullptr; i = i->next) {
        if (i->x == x) {
            if (i->v.get() == nullptr)
                throw RuntimeError("Variable used before initialization: " + x);
            return i->v;
        }
    }
    // Not found in the environment: primitives are first-class values.
    if (primitives.count(x)) {
        Value proc = makePrimitiveProcedure(primitives[x], e);
        if (proc.get() != nullptr)
            return proc;
    }
    throw RuntimeError("Undefined variable: " + x);
}

// ============================================================================
// Arithmetic
// ============================================================================

Value Plus::evalRator(const Value &rand1, const Value &rand2) { // +
    long long n1, d1, n2, d2;
    asFraction(rand1, n1, d1);
    asFraction(rand2, n2, d2);
    return makeNumber(n1 * d2 + n2 * d1, d1 * d2);
}

Value Minus::evalRator(const Value &rand1, const Value &rand2) { // -
    long long n1, d1, n2, d2;
    asFraction(rand1, n1, d1);
    asFraction(rand2, n2, d2);
    return makeNumber(n1 * d2 - n2 * d1, d1 * d2);
}

Value Mult::evalRator(const Value &rand1, const Value &rand2) { // *
    long long n1, d1, n2, d2;
    asFraction(rand1, n1, d1);
    asFraction(rand2, n2, d2);
    return makeNumber(n1 * n2, d1 * d2);
}

Value Div::evalRator(const Value &rand1, const Value &rand2) { // /
    long long n1, d1, n2, d2;
    asFraction(rand1, n1, d1);
    asFraction(rand2, n2, d2);
    if (n2 == 0)
        throw RuntimeError("Division by zero");
    return makeNumber(n1 * d2, d1 * n2);
}

Value Modulo::evalRator(const Value &rand1, const Value &rand2) { // modulo
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int dividend = dynamic_cast<Integer *>(rand1.get())->n;
        int divisor = dynamic_cast<Integer *>(rand2.get())->n;
        if (divisor == 0) {
            throw(RuntimeError("Division by zero"));
        }
        return IntegerV(dividend % divisor);
    }
    throw(RuntimeError("modulo is only defined for integers"));
}

Value PlusVar::evalRator(const std::vector<Value> &args) { // + with multiple args
    long long num = 0, den = 1;
    for (const auto &a : args) {
        long long n, d;
        asFraction(a, n, d);
        num = num * d + n * den;
        den = den * d;
    }
    return makeNumber(num, den);
}

Value MinusVar::evalRator(const std::vector<Value> &args) { // - with multiple args
    if (args.empty())
        throw RuntimeError("- requires at least one argument");
    long long num, den;
    asFraction(args[0], num, den);
    if (args.size() == 1)
        return makeNumber(-num, den);
    for (size_t i = 1; i < args.size(); ++i) {
        long long n, d;
        asFraction(args[i], n, d);
        num = num * d - n * den;
        den = den * d;
    }
    return makeNumber(num, den);
}

Value MultVar::evalRator(const std::vector<Value> &args) { // * with multiple args
    long long num = 1, den = 1;
    for (const auto &a : args) {
        long long n, d;
        asFraction(a, n, d);
        num *= n;
        den *= d;
    }
    return makeNumber(num, den);
}

Value DivVar::evalRator(const std::vector<Value> &args) { // / with multiple args
    if (args.empty())
        throw RuntimeError("/ requires at least one argument");
    long long num, den;
    asFraction(args[0], num, den);
    if (args.size() == 1) {
        if (num == 0)
            throw RuntimeError("Division by zero");
        return makeNumber(den, num);
    }
    for (size_t i = 1; i < args.size(); ++i) {
        long long n, d;
        asFraction(args[i], n, d);
        if (n == 0)
            throw RuntimeError("Division by zero");
        num = num * d;
        den = den * n;
    }
    return makeNumber(num, den);
}

Value Expt::evalRator(const Value &rand1, const Value &rand2) { // expt
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int base = dynamic_cast<Integer *>(rand1.get())->n;
        int exponent = dynamic_cast<Integer *>(rand2.get())->n;

        if (exponent < 0) {
            throw(RuntimeError("Negative exponent not supported for integers"));
        }
        if (base == 0 && exponent == 0) {
            throw(RuntimeError("0^0 is undefined"));
        }

        long long result = 1;
        long long b = base;
        int exp = exponent;

        while (exp > 0) {
            if (exp % 2 == 1) {
                result *= b;
                if (result > INT_MAX || result < INT_MIN) {
                    throw(RuntimeError("Integer overflow in expt"));
                }
            }
            b *= b;
            if (b > INT_MAX || b < INT_MIN) {
                if (exp > 1) {
                    throw(RuntimeError("Integer overflow in expt"));
                }
            }
            exp /= 2;
        }

        return IntegerV((int)result);
    }
    throw(RuntimeError("Wrong typename"));
}

// ============================================================================
// Comparison
// ============================================================================

//A FUNCTION TO SIMPLIFY THE COMPARISON WITH INTEGER AND RATIONAL NUMBER
int compareNumericValues(const Value &v1, const Value &v2) {
    if (v1->v_type == V_INT && v2->v_type == V_INT) {
        int n1 = dynamic_cast<Integer *>(v1.get())->n;
        int n2 = dynamic_cast<Integer *>(v2.get())->n;
        return (n1 < n2) ? -1 : (n1 > n2) ? 1 : 0;
    } else if (v1->v_type == V_RATIONAL && v2->v_type == V_INT) {
        Rational *r1 = dynamic_cast<Rational *>(v1.get());
        int n2 = dynamic_cast<Integer *>(v2.get())->n;
        long long left = r1->numerator;
        long long right = (long long)n2 * r1->denominator;
        return (left < right) ? -1 : (left > right) ? 1 : 0;
    } else if (v1->v_type == V_INT && v2->v_type == V_RATIONAL) {
        int n1 = dynamic_cast<Integer *>(v1.get())->n;
        Rational *r2 = dynamic_cast<Rational *>(v2.get());
        long long left = (long long)n1 * r2->denominator;
        long long right = r2->numerator;
        return (left < right) ? -1 : (left > right) ? 1 : 0;
    } else if (v1->v_type == V_RATIONAL && v2->v_type == V_RATIONAL) {
        Rational *r1 = dynamic_cast<Rational *>(v1.get());
        Rational *r2 = dynamic_cast<Rational *>(v2.get());
        long long left = (long long)r1->numerator * r2->denominator;
        long long right = (long long)r2->numerator * r1->denominator;
        return (left < right) ? -1 : (left > right) ? 1 : 0;
    }
    throw RuntimeError("Wrong typename in numeric comparison");
}

Value Less::evalRator(const Value &rand1, const Value &rand2) { // <
    return BooleanV(compareNumericValues(rand1, rand2) < 0);
}

Value LessEq::evalRator(const Value &rand1, const Value &rand2) { // <=
    return BooleanV(compareNumericValues(rand1, rand2) <= 0);
}

Value Equal::evalRator(const Value &rand1, const Value &rand2) { // =
    return BooleanV(compareNumericValues(rand1, rand2) == 0);
}

Value GreaterEq::evalRator(const Value &rand1, const Value &rand2) { // >=
    return BooleanV(compareNumericValues(rand1, rand2) >= 0);
}

Value Greater::evalRator(const Value &rand1, const Value &rand2) { // >
    return BooleanV(compareNumericValues(rand1, rand2) > 0);
}

// Helper for variadic comparisons: check that every adjacent pair satisfies the
// relation encoded by `cmp` acting on the sign of compareNumericValues.
template <typename Pred>
static Value chainCompare(const std::vector<Value> &args, Pred ok) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (!ok(compareNumericValues(args[i - 1], args[i])))
            return BooleanV(false);
    }
    return BooleanV(true);
}

Value LessVar::evalRator(const std::vector<Value> &args) { // < with multiple args
    return chainCompare(args, [](int c) { return c < 0; });
}

Value LessEqVar::evalRator(const std::vector<Value> &args) { // <= with multiple args
    return chainCompare(args, [](int c) { return c <= 0; });
}

Value EqualVar::evalRator(const std::vector<Value> &args) { // = with multiple args
    return chainCompare(args, [](int c) { return c == 0; });
}

Value GreaterEqVar::evalRator(const std::vector<Value> &args) { // >= with multiple args
    return chainCompare(args, [](int c) { return c >= 0; });
}

Value GreaterVar::evalRator(const std::vector<Value> &args) { // > with multiple args
    return chainCompare(args, [](int c) { return c > 0; });
}

// ============================================================================
// List operations
// ============================================================================

Value Cons::evalRator(const Value &rand1, const Value &rand2) { // cons
    return PairV(rand1, rand2);
}

Value ListFunc::evalRator(const std::vector<Value> &args) { // list function
    Value result = NullV();
    for (size_t i = args.size(); i > 0; --i)
        result = PairV(args[i - 1], result);
    return result;
}

Value IsList::evalRator(const Value &rand) { // list?
    Value cur = rand;
    while (cur->v_type == V_PAIR)
        cur = dynamic_cast<Pair *>(cur.get())->cdr;
    return BooleanV(cur->v_type == V_NULL);
}

Value Car::evalRator(const Value &rand) { // car
    if (rand->v_type != V_PAIR)
        throw RuntimeError("car: argument is not a pair");
    return dynamic_cast<Pair *>(rand.get())->car;
}

Value Cdr::evalRator(const Value &rand) { // cdr
    if (rand->v_type != V_PAIR)
        throw RuntimeError("cdr: argument is not a pair");
    return dynamic_cast<Pair *>(rand.get())->cdr;
}

Value SetCar::evalRator(const Value &rand1, const Value &rand2) { // set-car!
    if (rand1->v_type != V_PAIR)
        throw RuntimeError("set-car!: argument is not a pair");
    dynamic_cast<Pair *>(rand1.get())->car = rand2;
    return VoidV();
}

Value SetCdr::evalRator(const Value &rand1, const Value &rand2) { // set-cdr!
    if (rand1->v_type != V_PAIR)
        throw RuntimeError("set-cdr!: argument is not a pair");
    dynamic_cast<Pair *>(rand1.get())->cdr = rand2;
    return VoidV();
}

// ============================================================================
// Type predicates
// ============================================================================

Value IsEq::evalRator(const Value &rand1, const Value &rand2) { // eq?
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        return BooleanV((dynamic_cast<Integer *>(rand1.get())->n) == (dynamic_cast<Integer *>(rand2.get())->n));
    } else if (rand1->v_type == V_BOOL && rand2->v_type == V_BOOL) {
        return BooleanV((dynamic_cast<Boolean *>(rand1.get())->b) == (dynamic_cast<Boolean *>(rand2.get())->b));
    } else if (rand1->v_type == V_SYM && rand2->v_type == V_SYM) {
        return BooleanV((dynamic_cast<Symbol *>(rand1.get())->s) == (dynamic_cast<Symbol *>(rand2.get())->s));
    } else if ((rand1->v_type == V_NULL && rand2->v_type == V_NULL) ||
               (rand1->v_type == V_VOID && rand2->v_type == V_VOID)) {
        return BooleanV(true);
    } else {
        return BooleanV(rand1.get() == rand2.get());
    }
}

Value IsBoolean::evalRator(const Value &rand) { // boolean?
    return BooleanV(rand->v_type == V_BOOL);
}

Value IsFixnum::evalRator(const Value &rand) { // number?
    return BooleanV(rand->v_type == V_INT);
}

Value IsNull::evalRator(const Value &rand) { // null?
    return BooleanV(rand->v_type == V_NULL);
}

Value IsPair::evalRator(const Value &rand) { // pair?
    return BooleanV(rand->v_type == V_PAIR);
}

Value IsProcedure::evalRator(const Value &rand) { // procedure?
    return BooleanV(rand->v_type == V_PROC);
}

Value IsSymbol::evalRator(const Value &rand) { // symbol?
    return BooleanV(rand->v_type == V_SYM);
}

Value IsString::evalRator(const Value &rand) { // string?
    return BooleanV(rand->v_type == V_STRING);
}

// ============================================================================
// Control flow
// ============================================================================

static bool isFalseValue(const Value &v) {
    return v->v_type == V_BOOL && dynamic_cast<Boolean *>(v.get())->b == false;
}

Value Begin::eval(Assoc &e) {
    Value result = VoidV();
    for (auto &expr : es)
        result = expr->eval(e);
    return result;
}

// Convert a quoted syntax datum into a runtime value.
static Value quoteToValue(Syntax s);

static bool isDot(Syntax s) {
    SymbolSyntax *sym = dynamic_cast<SymbolSyntax *>(s.get());
    return sym != nullptr && sym->s == ".";
}

static Value quoteListToValue(std::vector<Syntax> &elems) {
    Value tail = NullV();
    int n = (int)elems.size();
    int start = n - 1;
    // Handle a dotted tail: (a b . c)
    if (n >= 3 && isDot(elems[n - 2])) {
        tail = quoteToValue(elems[n - 1]);
        start = n - 3;
    }
    for (int i = start; i >= 0; --i)
        tail = PairV(quoteToValue(elems[i]), tail);
    return tail;
}

static Value quoteToValue(Syntax s) {
    if (Number *num = dynamic_cast<Number *>(s.get()))
        return IntegerV(num->n);
    if (RationalSyntax *r = dynamic_cast<RationalSyntax *>(s.get())) {
        if (r->denominator == 1)
            return IntegerV(r->numerator);
        return RationalV(r->numerator, r->denominator);
    }
    if (dynamic_cast<TrueSyntax *>(s.get()))
        return BooleanV(true);
    if (dynamic_cast<FalseSyntax *>(s.get()))
        return BooleanV(false);
    if (StringSyntax *str = dynamic_cast<StringSyntax *>(s.get()))
        return StringV(str->s);
    if (SymbolSyntax *sym = dynamic_cast<SymbolSyntax *>(s.get()))
        return SymbolV(sym->s);
    if (List *lst = dynamic_cast<List *>(s.get()))
        return quoteListToValue(lst->stxs);
    throw RuntimeError("Cannot quote this syntax");
}

Value Quote::eval(Assoc &e) {
    return quoteToValue(s);
}

Value AndVar::eval(Assoc &e) { // and with short-circuit evaluation
    Value result = BooleanV(true);
    for (auto &r : rands) {
        result = r->eval(e);
        if (isFalseValue(result))
            return result;
    }
    return result;
}

Value OrVar::eval(Assoc &e) { // or with short-circuit evaluation
    Value result = BooleanV(false);
    for (auto &r : rands) {
        result = r->eval(e);
        if (!isFalseValue(result))
            return result;
    }
    return result;
}

Value Not::evalRator(const Value &rand) { // not
    return BooleanV(isFalseValue(rand));
}

Value If::eval(Assoc &e) {
    Value c = cond->eval(e);
    if (isFalseValue(c))
        return alter->eval(e);
    return conseq->eval(e);
}

Value Cond::eval(Assoc &env) {
    for (auto &clause : clauses) {
        Value test = clause[0]->eval(env);
        if (!isFalseValue(test)) {
            if (clause.size() == 1)
                return test;
            Value result = test;
            for (size_t i = 1; i < clause.size(); ++i)
                result = clause[i]->eval(env);
            return result;
        }
    }
    return VoidV();
}

// ============================================================================
// Variables, functions and binding constructs
// ============================================================================

Value Lambda::eval(Assoc &env) {
    return ProcedureV(x, e, env);
}

Value Apply::eval(Assoc &e) {
    Value ratorVal = rator->eval(e);
    if (ratorVal->v_type != V_PROC)
        throw RuntimeError("Attempt to apply a non-procedure");
    Procedure *clos = dynamic_cast<Procedure *>(ratorVal.get());

    std::vector<Value> args;
    args.reserve(rand.size());
    for (auto &r : rand)
        args.push_back(r->eval(e));

    // First-class variadic / short-circuit primitives.
    if (clos->parameters.size() == 1 && clos->parameters[0] == VARIADIC_SENTINEL) {
        if (Variadic *v = dynamic_cast<Variadic *>(clos->e.get()))
            return v->evalRator(args);
        if (dynamic_cast<AndVar *>(clos->e.get())) {
            Value r = BooleanV(true);
            for (auto &a : args) {
                if (isFalseValue(a)) return a;
                r = a;
            }
            return r;
        }
        if (dynamic_cast<OrVar *>(clos->e.get())) {
            Value r = BooleanV(false);
            for (auto &a : args) {
                if (!isFalseValue(a)) return a;
                r = a;
            }
            return r;
        }
    }

    if (args.size() != clos->parameters.size())
        throw RuntimeError("Wrong number of arguments");

    Assoc param_env = clos->env;
    for (size_t i = 0; i < args.size(); ++i)
        param_env = extend(clos->parameters[i], args[i], param_env);

    return clos->e->eval(param_env);
}

Value Define::eval(Assoc &env) {
    // If the name already exists, reuse its slot in place. This both supports
    // redefinition and keeps the binding reachable from closures that captured
    // the environment earlier.
    for (Assoc i = env; i.get() != nullptr; i = i->next) {
        if (i->x == var) {
            i->v = Value(nullptr);       // placeholder for recursion
            i->v = e->eval(env);
            return VoidV();
        }
    }
    // A brand-new binding. The very first binding has to become the new head
    // of the (previously empty) environment.
    if (env.get() == nullptr) {
        env = extend(var, Value(nullptr), env);
        Value v = e->eval(env);
        modify(var, v, env);
        return VoidV();
    }
    // Otherwise append at the tail so that procedures created earlier (which
    // captured a node in this shared chain) can still observe the new binding.
    // This is what makes mutually-recursive top-level definitions work.
    Assoc tail = env;
    while (tail->next.get() != nullptr)
        tail = tail->next;
    Assoc emptyTail = empty();
    Assoc node = extend(var, Value(nullptr), emptyTail);
    tail->next = node;
    Value v = e->eval(env);
    node->v = v;
    return VoidV();
}

Value Let::eval(Assoc &env) {
    // All initialisers are evaluated in the enclosing environment.
    std::vector<Value> vals;
    vals.reserve(bind.size());
    for (auto &b : bind)
        vals.push_back(b.second->eval(env));
    Assoc newEnv = env;
    for (size_t i = 0; i < bind.size(); ++i)
        newEnv = extend(bind[i].first, vals[i], newEnv);
    return body->eval(newEnv);
}

Value Letrec::eval(Assoc &env) {
    // Phase 1: bind every name to an uninitialised placeholder.
    Assoc newEnv = env;
    for (auto &b : bind)
        newEnv = extend(b.first, Value(nullptr), newEnv);
    // Phase 2: evaluate every initialiser in that environment. Using a name
    // before it is filled in (outside a closure) raises a RuntimeError.
    std::vector<Value> vals;
    vals.reserve(bind.size());
    for (auto &b : bind)
        vals.push_back(b.second->eval(newEnv));
    // Phase 3: update the bindings in place so closures observe final values.
    for (size_t i = 0; i < bind.size(); ++i)
        modify(bind[i].first, vals[i], newEnv);
    return body->eval(newEnv);
}

Value Set::eval(Assoc &env) {
    for (Assoc i = env; i.get() != nullptr; i = i->next) {
        if (i->x == var) {
            i->v = e->eval(env);
            return VoidV();
        }
    }
    throw RuntimeError("set!: undefined variable: " + var);
}

Value Display::evalRator(const Value &rand) { // display function
    if (rand->v_type == V_STRING) {
        String *str_ptr = dynamic_cast<String *>(rand.get());
        std::cout << str_ptr->s;
    } else {
        rand->show(std::cout);
    }
    return VoidV();
}
