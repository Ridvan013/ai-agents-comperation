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
#include <cstdlib>
#include <vector>
#include <map>
#include <climits>

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

// ============================================================================
// Numeric helpers (integers and rationals)
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

// Build a normalized numeric value; reduces to Integer when denominator is 1.
static Value makeNumber(long long num, long long den) {
    if (den == 0)
        throw RuntimeError("Division by zero");
    if (den < 0) {
        num = -num;
        den = -den;
    }
    long long g = gcdll(num, den);
    if (g != 0) {
        num /= g;
        den /= g;
    }
    if (den == 1)
        return IntegerV((int)num);
    return RationalV((int)num, (int)den);
}

static Value numAdd(const Value &a, const Value &b) {
    long long an, ad, bn, bd;
    asFraction(a, an, ad);
    asFraction(b, bn, bd);
    return makeNumber(an * bd + bn * ad, ad * bd);
}

static Value numSub(const Value &a, const Value &b) {
    long long an, ad, bn, bd;
    asFraction(a, an, ad);
    asFraction(b, bn, bd);
    return makeNumber(an * bd - bn * ad, ad * bd);
}

static Value numMul(const Value &a, const Value &b) {
    long long an, ad, bn, bd;
    asFraction(a, an, ad);
    asFraction(b, bn, bd);
    return makeNumber(an * bn, ad * bd);
}

static Value numDiv(const Value &a, const Value &b) {
    long long an, ad, bn, bd;
    asFraction(a, an, ad);
    asFraction(b, bn, bd);
    if (bn == 0)
        throw RuntimeError("Division by zero");
    return makeNumber(an * bd, ad * bn);
}

// true when the value is anything other than boolean #f
static bool isTruthy(const Value &v) {
    return !(v->v_type == V_BOOL && dynamic_cast<Boolean *>(v.get())->b == false);
}

// ============================================================================
// Literals
// ============================================================================

Value Fixnum::eval(Assoc &e) { // evaluation of a fixnum
    return IntegerV(n);
}

Value RationalNum::eval(Assoc &e) { // evaluation of a rational number
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
    for (auto &r : rands)
        args.push_back(r->eval(e));
    return evalRator(args);
}

// ============================================================================
// Variable reference
// ============================================================================

Value Var::eval(Assoc &e) { // evaluation of variable
    Value matched_value = find(x, e);
    if (matched_value.get() != nullptr) {
        return matched_value;
    }

    // Not bound in the environment. If it names a primitive, materialize a
    // closure so the primitive can be used as a first-class value.
    if (primitives.count(x)) {
        static std::map<ExprType, std::pair<Expr, std::vector<std::string>>> primitive_map = {
            {E_VOID,     {new MakeVoid(), {}}},
            {E_EXIT,     {new Exit(), {}}},
            {E_BOOLQ,    {new IsBoolean(new Var("parm")), {"parm"}}},
            {E_INTQ,     {new IsFixnum(new Var("parm")), {"parm"}}},
            {E_NULLQ,    {new IsNull(new Var("parm")), {"parm"}}},
            {E_PAIRQ,    {new IsPair(new Var("parm")), {"parm"}}},
            {E_PROCQ,    {new IsProcedure(new Var("parm")), {"parm"}}},
            {E_SYMBOLQ,  {new IsSymbol(new Var("parm")), {"parm"}}},
            {E_LISTQ,    {new IsList(new Var("parm")), {"parm"}}},
            {E_STRINGQ,  {new IsString(new Var("parm")), {"parm"}}},
            {E_DISPLAY,  {new Display(new Var("parm")), {"parm"}}},
            {E_NOT,      {new Not(new Var("parm")), {"parm"}}},
            {E_CAR,      {new Car(new Var("parm")), {"parm"}}},
            {E_CDR,      {new Cdr(new Var("parm")), {"parm"}}},
            {E_CONS,     {new Cons(new Var("parm1"), new Var("parm2")), {"parm1", "parm2"}}},
            {E_SETCAR,   {new SetCar(new Var("parm1"), new Var("parm2")), {"parm1", "parm2"}}},
            {E_SETCDR,   {new SetCdr(new Var("parm1"), new Var("parm2")), {"parm1", "parm2"}}},
            {E_EQQ,      {new IsEq(new Var("parm1"), new Var("parm2")), {"parm1", "parm2"}}},
            {E_MODULO,   {new Modulo(new Var("parm1"), new Var("parm2")), {"parm1", "parm2"}}},
            {E_EXPT,     {new Expt(new Var("parm1"), new Var("parm2")), {"parm1", "parm2"}}},
            {E_PLUS,     {new PlusVar({}),  {}}},
            {E_MINUS,    {new MinusVar({}), {}}},
            {E_MUL,      {new MultVar({}),  {}}},
            {E_DIV,      {new DivVar({}),   {}}},
            {E_LT,       {new LessVar({}),      {}}},
            {E_LE,       {new LessEqVar({}),    {}}},
            {E_EQ,       {new EqualVar({}),     {}}},
            {E_GE,       {new GreaterEqVar({}), {}}},
            {E_GT,       {new GreaterVar({}),   {}}},
            {E_LIST,     {new ListFunc({}), {}}},
        };

        auto it = primitive_map.find(primitives[x]);
        if (it != primitive_map.end()) {
            return ProcedureV(it->second.second, it->second.first, e);
        }
    }
    throw RuntimeError("undefined variable: " + x);
}

// ============================================================================
// Arithmetic
// ============================================================================

Value Plus::evalRator(const Value &rand1, const Value &rand2) { // +
    return numAdd(rand1, rand2);
}

Value Minus::evalRator(const Value &rand1, const Value &rand2) { // -
    return numSub(rand1, rand2);
}

Value Mult::evalRator(const Value &rand1, const Value &rand2) { // *
    return numMul(rand1, rand2);
}

Value Div::evalRator(const Value &rand1, const Value &rand2) { // /
    return numDiv(rand1, rand2);
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
    Value acc = IntegerV(0);
    for (auto &a : args)
        acc = numAdd(acc, a);
    return acc;
}

Value MinusVar::evalRator(const std::vector<Value> &args) { // - with multiple args
    if (args.empty())
        throw RuntimeError("- requires at least one argument");
    if (args.size() == 1)
        return numSub(IntegerV(0), args[0]);
    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
        acc = numSub(acc, args[i]);
    return acc;
}

Value MultVar::evalRator(const std::vector<Value> &args) { // * with multiple args
    Value acc = IntegerV(1);
    for (auto &a : args)
        acc = numMul(acc, a);
    return acc;
}

Value DivVar::evalRator(const std::vector<Value> &args) { // / with multiple args
    if (args.empty())
        throw RuntimeError("/ requires at least one argument");
    if (args.size() == 1)
        return numDiv(IntegerV(1), args[0]);
    Value acc = args[0];
    for (size_t i = 1; i < args.size(); i++)
        acc = numDiv(acc, args[i]);
    return acc;
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
// Comparisons
// ============================================================================

// A function to simplify the comparison with integer and rational numbers.
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

Value LessVar::evalRator(const std::vector<Value> &args) { // < with multiple args
    for (size_t i = 0; i + 1 < args.size(); i++)
        if (!(compareNumericValues(args[i], args[i + 1]) < 0))
            return BooleanV(false);
    return BooleanV(true);
}

Value LessEqVar::evalRator(const std::vector<Value> &args) { // <= with multiple args
    for (size_t i = 0; i + 1 < args.size(); i++)
        if (!(compareNumericValues(args[i], args[i + 1]) <= 0))
            return BooleanV(false);
    return BooleanV(true);
}

Value EqualVar::evalRator(const std::vector<Value> &args) { // = with multiple args
    for (size_t i = 0; i + 1 < args.size(); i++)
        if (compareNumericValues(args[i], args[i + 1]) != 0)
            return BooleanV(false);
    return BooleanV(true);
}

Value GreaterEqVar::evalRator(const std::vector<Value> &args) { // >= with multiple args
    for (size_t i = 0; i + 1 < args.size(); i++)
        if (!(compareNumericValues(args[i], args[i + 1]) >= 0))
            return BooleanV(false);
    return BooleanV(true);
}

Value GreaterVar::evalRator(const std::vector<Value> &args) { // > with multiple args
    for (size_t i = 0; i + 1 < args.size(); i++)
        if (!(compareNumericValues(args[i], args[i + 1]) > 0))
            return BooleanV(false);
    return BooleanV(true);
}

// ============================================================================
// Pairs and lists
// ============================================================================

Value Cons::evalRator(const Value &rand1, const Value &rand2) { // cons
    return PairV(rand1, rand2);
}

Value ListFunc::evalRator(const std::vector<Value> &args) { // list function
    Value result = NullV();
    for (int i = (int)args.size() - 1; i >= 0; i--)
        result = PairV(args[i], result);
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
        throw RuntimeError("car: not a pair");
    return dynamic_cast<Pair *>(rand.get())->car;
}

Value Cdr::evalRator(const Value &rand) { // cdr
    if (rand->v_type != V_PAIR)
        throw RuntimeError("cdr: not a pair");
    return dynamic_cast<Pair *>(rand.get())->cdr;
}

Value SetCar::evalRator(const Value &rand1, const Value &rand2) { // set-car!
    if (rand1->v_type != V_PAIR)
        throw RuntimeError("set-car!: not a pair");
    dynamic_cast<Pair *>(rand1.get())->car = rand2;
    return VoidV();
}

Value SetCdr::evalRator(const Value &rand1, const Value &rand2) { // set-cdr!
    if (rand1->v_type != V_PAIR)
        throw RuntimeError("set-cdr!: not a pair");
    dynamic_cast<Pair *>(rand1.get())->cdr = rand2;
    return VoidV();
}

// ============================================================================
// Type predicates
// ============================================================================

Value IsEq::evalRator(const Value &rand1, const Value &rand2) { // eq?
    // Check if type is Integer
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        return BooleanV((dynamic_cast<Integer *>(rand1.get())->n) == (dynamic_cast<Integer *>(rand2.get())->n));
    }
    // Check if type is Boolean
    else if (rand1->v_type == V_BOOL && rand2->v_type == V_BOOL) {
        return BooleanV((dynamic_cast<Boolean *>(rand1.get())->b) == (dynamic_cast<Boolean *>(rand2.get())->b));
    }
    // Check if type is Symbol
    else if (rand1->v_type == V_SYM && rand2->v_type == V_SYM) {
        return BooleanV((dynamic_cast<Symbol *>(rand1.get())->s) == (dynamic_cast<Symbol *>(rand2.get())->s));
    }
    // Check if type is Null or Void
    else if ((rand1->v_type == V_NULL && rand2->v_type == V_NULL) ||
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

Value Begin::eval(Assoc &e) {
    Value v = VoidV();
    for (auto &ex : es)
        v = ex->eval(e);
    return v;
}

// Convert a quoted syntax tree into a runtime value.
static Value quoteToValue(const Syntax &s);

static Value listSyntaxToValue(List *lst) {
    std::vector<Syntax> &xs = lst->stxs;
    int n = (int)xs.size();
    Value tail = NullV();
    int limit = n;

    // Detect a dotted tail: "<...> . x" where '.' is the penultimate element.
    if (n >= 3) {
        SymbolSyntax *dot = dynamic_cast<SymbolSyntax *>(xs[n - 2].get());
        if (dot != nullptr && dot->s == ".") {
            tail = quoteToValue(xs[n - 1]);
            limit = n - 2;
        }
    }

    Value result = tail;
    for (int i = limit - 1; i >= 0; i--)
        result = PairV(quoteToValue(xs[i]), result);
    return result;
}

static Value quoteToValue(const Syntax &s) {
    SyntaxBase *p = s.get();
    if (Number *num = dynamic_cast<Number *>(p))
        return IntegerV(num->n);
    if (RationalSyntax *r = dynamic_cast<RationalSyntax *>(p))
        return RationalV(r->numerator, r->denominator);
    if (dynamic_cast<TrueSyntax *>(p))
        return BooleanV(true);
    if (dynamic_cast<FalseSyntax *>(p))
        return BooleanV(false);
    if (StringSyntax *str = dynamic_cast<StringSyntax *>(p))
        return StringV(str->s);
    if (SymbolSyntax *sym = dynamic_cast<SymbolSyntax *>(p))
        return SymbolV(sym->s);
    if (List *lst = dynamic_cast<List *>(p))
        return listSyntaxToValue(lst);
    throw RuntimeError("quote: unsupported syntax");
}

Value Quote::eval(Assoc &e) {
    return quoteToValue(s);
}

Value AndVar::eval(Assoc &e) { // and with short-circuit evaluation
    if (rands.empty())
        return BooleanV(true);
    Value v = BooleanV(true);
    for (auto &r : rands) {
        v = r->eval(e);
        if (!isTruthy(v))
            return BooleanV(false);
    }
    return v;
}

Value OrVar::eval(Assoc &e) { // or with short-circuit evaluation
    if (rands.empty())
        return BooleanV(false);
    Value v = BooleanV(false);
    for (auto &r : rands) {
        v = r->eval(e);
        if (isTruthy(v))
            return v;
    }
    return v;
}

Value Not::evalRator(const Value &rand) { // not
    return BooleanV(!isTruthy(rand));
}

Value If::eval(Assoc &e) {
    Value c = cond->eval(e);
    if (isTruthy(c))
        return conseq->eval(e);
    return alter->eval(e);
}

Value Cond::eval(Assoc &env) {
    for (auto &clause : clauses) {
        Value t = clause[0]->eval(env);
        if (!isTruthy(t))
            continue;
        if (clause.size() == 1)
            return t; // condition-only clause: return the condition value
        Value v = VoidV();
        for (size_t i = 1; i < clause.size(); i++)
            v = clause[i]->eval(env);
        return v;
    }
    return VoidV();
}

// ============================================================================
// Functions, definitions, bindings, assignment
// ============================================================================

Value Lambda::eval(Assoc &env) {
    return ProcedureV(x, e, env);
}

Value Apply::eval(Assoc &e) {
    Value ratorV = rator->eval(e);
    if (ratorV->v_type != V_PROC) {
        throw RuntimeError("Attempt to apply a non-procedure");
    }
    Procedure *clos_ptr = dynamic_cast<Procedure *>(ratorV.get());

    // Evaluate the actual arguments in the caller's environment.
    std::vector<Value> args;
    for (auto &r : rand)
        args.push_back(r->eval(e));

    // Primitive stored as a first-class value: its body is a variadic node
    // with no fixed parameters and no captured operands. Apply it directly.
    if (auto varNode = dynamic_cast<Variadic *>(clos_ptr->e.get())) {
        if (clos_ptr->parameters.empty() && varNode->rands.empty()) {
            return varNode->evalRator(args);
        }
    }

    if (args.size() != clos_ptr->parameters.size())
        throw RuntimeError("Wrong number of arguments");

    Assoc param_env = clos_ptr->env;
    for (size_t i = 0; i < args.size(); i++)
        param_env = extend(clos_ptr->parameters[i], args[i], param_env);

    return clos_ptr->e->eval(param_env);
}

Value Define::eval(Assoc &env) {
    // define always targets the (global) frame. To make recursion work and to
    // let previously-created closures observe later definitions (e.g. mutually
    // recursive functions), we keep a single binding node per name:
    //   - if the name is new, append a placeholder node at the TAIL of the
    //     frame so every closure sharing this chain can reach it;
    //   - if it already exists, reuse that node (redefinition updates it).
    // The value is computed first (the placeholder / old value is in scope for
    // recursive lambdas, which capture the node), then written into the node.
    if (find(var, env).get() == nullptr) {
        if (env.get() == nullptr) {
            env = extend(var, VoidV(), env); // create the head node
        } else {
            Assoc cur = env;
            while (cur->next.get() != nullptr)
                cur = cur->next;
            Assoc nul = empty();
            cur->next = extend(var, VoidV(), nul); // append at the tail
        }
    }
    Value v = e->eval(env);
    modify(var, v, env);
    return VoidV();
}

Value Let::eval(Assoc &env) {
    // Evaluate all initializers in the OUTER environment.
    std::vector<Value> vals;
    for (auto &b : bind)
        vals.push_back(b.second->eval(env));

    Assoc newEnv = env;
    for (size_t i = 0; i < bind.size(); i++)
        newEnv = extend(bind[i].first, vals[i], newEnv);

    return body->eval(newEnv);
}

Value Letrec::eval(Assoc &env) {
    // Step 1: bind every name to an "unusable" placeholder (nullptr value).
    Assoc env1 = env;
    for (auto &b : bind)
        env1 = extend(b.first, Value(nullptr), env1);

    // Step 2: evaluate all initializers with the placeholders in scope.
    std::vector<Value> vals;
    for (auto &b : bind)
        vals.push_back(b.second->eval(env1));

    // Step 3: update the bindings in place; closures captured env1 see them.
    for (size_t i = 0; i < bind.size(); i++)
        modify(bind[i].first, vals[i], env1);

    return body->eval(env1);
}

Value Set::eval(Assoc &env) {
    Value v = e->eval(env);
    if (find(var, env).get() == nullptr)
        throw RuntimeError("set!: undefined variable " + var);
    modify(var, v, env);
    return VoidV();
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
