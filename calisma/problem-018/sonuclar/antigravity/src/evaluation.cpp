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
#include <iostream>

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

// Helper: GCD
static long long eval_gcd(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

static Value makeNumeric(long long num, long long den = 1) {
    if (den == 0) throw RuntimeError("Division by zero");
    long long g = eval_gcd(num, den);
    num /= g;
    den /= g;
    if (den < 0) { num = -num; den = -den; }
    if (den == 1) return IntegerV((int)num);
    return RationalV((int)num, (int)den);
}

static void getNumDen(const Value &v, long long &num, long long &den) {
    if (v->v_type == V_INT) {
        num = dynamic_cast<Integer*>(v.get())->n;
        den = 1;
    } else if (v->v_type == V_RATIONAL) {
        Rational* r = dynamic_cast<Rational*>(v.get());
        num = r->numerator;
        den = r->denominator;
    } else {
        throw RuntimeError("Expected number");
    }
}

Value Fixnum::eval(Assoc &e) { return IntegerV(n); }
Value RationalNum::eval(Assoc &e) { return RationalV(numerator, denominator); }
Value StringExpr::eval(Assoc &e) { return StringV(s); }
Value True::eval(Assoc &e) { return BooleanV(true); }
Value False::eval(Assoc &e) { return BooleanV(false); }
Value MakeVoid::eval(Assoc &e) { return VoidV(); }
Value Exit::eval(Assoc &e) { return TerminateV(); }

Value Unary::eval(Assoc &e) { return evalRator(rand->eval(e)); }
Value Binary::eval(Assoc &e) { return evalRator(rand1->eval(e), rand2->eval(e)); }

Value Variadic::eval(Assoc &e) {
    std::vector<Value> evaluated_rands;
    for (auto &expr : rands) {
        evaluated_rands.push_back(expr->eval(e));
    }
    return evalRator(evaluated_rands);
}

Value Var::eval(Assoc &e) {
    if (x.empty()) throw RuntimeError("Invalid empty variable name");
    char first = x[0];
    if (isdigit(first) || first == '.' || first == '@') {
        throw RuntimeError("Invalid variable name: " + x);
    }
    
    Value matched_value = find(x, e);
    if (matched_value.get() != nullptr) {
        return matched_value;
    }
    
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
                {E_STRINGQ,  {new IsString(new Var("parm")), {"parm"}}},
                {E_LISTQ,    {new IsList(new Var("parm")), {"parm"}}},
                {E_DISPLAY,  {new Display(new Var("parm")), {"parm"}}},
                {E_PLUS,     {new PlusVar({}),  {}}},
                {E_MINUS,    {new MinusVar({}), {}}},
                {E_MUL,      {new MultVar({}),  {}}},
                {E_DIV,      {new DivVar({}),   {}}},
                {E_MODULO,   {new Modulo(new Var("parm1"), new Var("parm2")), {"parm1","parm2"}}},
                {E_EXPT,     {new Expt(new Var("parm1"), new Var("parm2")), {"parm1","parm2"}}},
                {E_EQQ,      {new IsEq(new Var("parm1"), new Var("parm2")), {"parm1","parm2"}}},
                {E_LT,       {new LessVar({}), {}}},
                {E_LE,       {new LessEqVar({}), {}}},
                {E_EQ,       {new EqualVar({}), {}}},
                {E_GE,       {new GreaterEqVar({}), {}}},
                {E_GT,       {new GreaterVar({}), {}}},
                {E_CONS,     {new Cons(new Var("parm1"), new Var("parm2")), {"parm1","parm2"}}},
                {E_CAR,      {new Car(new Var("parm")), {"parm"}}},
                {E_CDR,      {new Cdr(new Var("parm")), {"parm"}}},
                {E_LIST,     {new ListFunc({}), {}}},
                {E_SETCAR,   {new SetCar(new Var("parm1"), new Var("parm2")), {"parm1","parm2"}}},
                {E_SETCDR,   {new SetCdr(new Var("parm1"), new Var("parm2")), {"parm1","parm2"}}},
                {E_NOT,      {new Not(new Var("parm")), {"parm"}}},
        };

        auto it = primitive_map.find(primitives[x]);
        if (it != primitive_map.end()) {
            return ProcedureV(it->second.second, it->second.first, empty());
        }
    }
    throw RuntimeError("Variable undefined: " + x);
}

Value Plus::evalRator(const Value &rand1, const Value &rand2) {
    long long n1, d1, n2, d2;
    getNumDen(rand1, n1, d1);
    getNumDen(rand2, n2, d2);
    return makeNumeric(n1 * d2 + n2 * d1, d1 * d2);
}

Value Minus::evalRator(const Value &rand1, const Value &rand2) {
    long long n1, d1, n2, d2;
    getNumDen(rand1, n1, d1);
    getNumDen(rand2, n2, d2);
    return makeNumeric(n1 * d2 - n2 * d1, d1 * d2);
}

Value Mult::evalRator(const Value &rand1, const Value &rand2) {
    long long n1, d1, n2, d2;
    getNumDen(rand1, n1, d1);
    getNumDen(rand2, n2, d2);
    return makeNumeric(n1 * n2, d1 * d2);
}

Value Div::evalRator(const Value &rand1, const Value &rand2) {
    long long n1, d1, n2, d2;
    getNumDen(rand1, n1, d1);
    getNumDen(rand2, n2, d2);
    return makeNumeric(n1 * d2, d1 * n2);
}

Value Modulo::evalRator(const Value &rand1, const Value &rand2) { // modulo
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int dividend = dynamic_cast<Integer*>(rand1.get())->n;
        int divisor = dynamic_cast<Integer*>(rand2.get())->n;
        if (divisor == 0) {
            throw(RuntimeError("Division by zero"));
        }
        return IntegerV(dividend % divisor);
    }
    throw(RuntimeError("modulo is only defined for integers"));
}

Value PlusVar::evalRator(const std::vector<Value> &args) {
    if (args.empty()) return IntegerV(0);
    long long res_n = 0, res_d = 1;
    for (auto &arg : args) {
        long long n, d;
        getNumDen(arg, n, d);
        long long new_n = res_n * d + n * res_d;
        long long new_d = res_d * d;
        long long g = eval_gcd(new_n, new_d);
        res_n = new_n / g;
        res_d = new_d / g;
    }
    return makeNumeric(res_n, res_d);
}

Value MinusVar::evalRator(const std::vector<Value> &args) {
    if (args.empty()) throw RuntimeError("Wrong number of arguments for -");
    if (args.size() == 1) {
        long long n, d;
        getNumDen(args[0], n, d);
        return makeNumeric(-n, d);
    }
    long long res_n, res_d;
    getNumDen(args[0], res_n, res_d);
    for (size_t i = 1; i < args.size(); ++i) {
        long long n, d;
        getNumDen(args[i], n, d);
        long long new_n = res_n * d - n * res_d;
        long long new_d = res_d * d;
        long long g = eval_gcd(new_n, new_d);
        res_n = new_n / g;
        res_d = new_d / g;
    }
    return makeNumeric(res_n, res_d);
}

Value MultVar::evalRator(const std::vector<Value> &args) {
    if (args.empty()) return IntegerV(1);
    long long res_n = 1, res_d = 1;
    for (auto &arg : args) {
        long long n, d;
        getNumDen(arg, n, d);
        long long new_n = res_n * n;
        long long new_d = res_d * d;
        long long g = eval_gcd(new_n, new_d);
        res_n = new_n / g;
        res_d = new_d / g;
    }
    return makeNumeric(res_n, res_d);
}

Value DivVar::evalRator(const std::vector<Value> &args) {
    if (args.empty()) throw RuntimeError("Wrong number of arguments for /");
    if (args.size() == 1) {
        long long n, d;
        getNumDen(args[0], n, d);
        return makeNumeric(d, n);
    }
    long long res_n, res_d;
    getNumDen(args[0], res_n, res_d);
    for (size_t i = 1; i < args.size(); ++i) {
        long long n, d;
        getNumDen(args[i], n, d);
        long long new_n = res_n * d;
        long long new_d = res_d * n;
        long long g = eval_gcd(new_n, new_d);
        res_n = new_n / g;
        res_d = new_d / g;
    }
    return makeNumeric(res_n, res_d);
}

Value Expt::evalRator(const Value &rand1, const Value &rand2) { // expt
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int base = dynamic_cast<Integer*>(rand1.get())->n;
        int exponent = dynamic_cast<Integer*>(rand2.get())->n;
        
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
    throw(RuntimeError("Wrong typename for expt"));
}

int compareNumericValues(const Value &v1, const Value &v2) {
    if (v1->v_type == V_INT && v2->v_type == V_INT) {
        int n1 = dynamic_cast<Integer*>(v1.get())->n;
        int n2 = dynamic_cast<Integer*>(v2.get())->n;
        return (n1 < n2) ? -1 : (n1 > n2) ? 1 : 0;
    }
    else if (v1->v_type == V_RATIONAL && v2->v_type == V_INT) {
        Rational* r1 = dynamic_cast<Rational*>(v1.get());
        int n2 = dynamic_cast<Integer*>(v2.get())->n;
        long long left = r1->numerator;
        long long right = (long long)n2 * r1->denominator;
        return (left < right) ? -1 : (left > right) ? 1 : 0;
    }
    else if (v1->v_type == V_INT && v2->v_type == V_RATIONAL) {
        int n1 = dynamic_cast<Integer*>(v1.get())->n;
        Rational* r2 = dynamic_cast<Rational*>(v2.get());
        long long left = (long long)n1 * r2->denominator;
        long long right = r2->numerator;
        return (left < right) ? -1 : (left > right) ? 1 : 0;
    }
    else if (v1->v_type == V_RATIONAL && v2->v_type == V_RATIONAL) {
        Rational* r1 = dynamic_cast<Rational*>(v1.get());
        Rational* r2 = dynamic_cast<Rational*>(v2.get());
        long long left = (long long)r1->numerator * r2->denominator;
        long long right = (long long)r2->numerator * r1->denominator;
        return (left < right) ? -1 : (left > right) ? 1 : 0;
    }
    throw RuntimeError("Wrong typename in numeric comparison");
}

Value Less::evalRator(const Value &rand1, const Value &rand2) { return BooleanV(compareNumericValues(rand1, rand2) < 0); }
Value LessEq::evalRator(const Value &rand1, const Value &rand2) { return BooleanV(compareNumericValues(rand1, rand2) <= 0); }
Value Equal::evalRator(const Value &rand1, const Value &rand2) { return BooleanV(compareNumericValues(rand1, rand2) == 0); }
Value GreaterEq::evalRator(const Value &rand1, const Value &rand2) { return BooleanV(compareNumericValues(rand1, rand2) >= 0); }
Value Greater::evalRator(const Value &rand1, const Value &rand2) { return BooleanV(compareNumericValues(rand1, rand2) > 0); }

Value LessVar::evalRator(const std::vector<Value> &args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i-1], args[i]) >= 0) return BooleanV(false);
    }
    return BooleanV(true);
}
Value LessEqVar::evalRator(const std::vector<Value> &args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i-1], args[i]) > 0) return BooleanV(false);
    }
    return BooleanV(true);
}
Value EqualVar::evalRator(const std::vector<Value> &args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i-1], args[i]) != 0) return BooleanV(false);
    }
    return BooleanV(true);
}
Value GreaterEqVar::evalRator(const std::vector<Value> &args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i-1], args[i]) < 0) return BooleanV(false);
    }
    return BooleanV(true);
}
Value GreaterVar::evalRator(const std::vector<Value> &args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (compareNumericValues(args[i-1], args[i]) <= 0) return BooleanV(false);
    }
    return BooleanV(true);
}

Value Cons::evalRator(const Value &rand1, const Value &rand2) { return PairV(rand1, rand2); }
Value ListFunc::evalRator(const std::vector<Value> &args) {
    Value res = NullV();
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        res = PairV(*it, res);
    }
    return res;
}
Value IsList::evalRator(const Value &rand) {
    Value curr = rand;
    while (curr->v_type == V_PAIR) {
        curr = dynamic_cast<Pair*>(curr.get())->cdr;
    }
    return BooleanV(curr->v_type == V_NULL);
}
Value Car::evalRator(const Value &rand) {
    if (rand->v_type != V_PAIR) throw RuntimeError("car expects a pair");
    return dynamic_cast<Pair*>(rand.get())->car;
}
Value Cdr::evalRator(const Value &rand) {
    if (rand->v_type != V_PAIR) throw RuntimeError("cdr expects a pair");
    return dynamic_cast<Pair*>(rand.get())->cdr;
}
Value SetCar::evalRator(const Value &rand1, const Value &rand2) {
    if (rand1->v_type != V_PAIR) throw RuntimeError("set-car! expects a pair");
    dynamic_cast<Pair*>(rand1.get())->car = rand2;
    return VoidV();
}
Value SetCdr::evalRator(const Value &rand1, const Value &rand2) {
    if (rand1->v_type != V_PAIR) throw RuntimeError("set-cdr! expects a pair");
    dynamic_cast<Pair*>(rand1.get())->cdr = rand2;
    return VoidV();
}

Value IsEq::evalRator(const Value &rand1, const Value &rand2) { // eq?
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        return BooleanV((dynamic_cast<Integer*>(rand1.get())->n) == (dynamic_cast<Integer*>(rand2.get())->n));
    }
    else if (rand1->v_type == V_BOOL && rand2->v_type == V_BOOL) {
        return BooleanV((dynamic_cast<Boolean*>(rand1.get())->b) == (dynamic_cast<Boolean*>(rand2.get())->b));
    }
    else if (rand1->v_type == V_SYM && rand2->v_type == V_SYM) {
        return BooleanV((dynamic_cast<Symbol*>(rand1.get())->s) == (dynamic_cast<Symbol*>(rand2.get())->s));
    }
    else if ((rand1->v_type == V_NULL && rand2->v_type == V_NULL) ||
             (rand1->v_type == V_VOID && rand2->v_type == V_VOID)) {
        return BooleanV(true);
    } else {
        return BooleanV(rand1.get() == rand2.get());
    }
}

Value IsBoolean::evalRator(const Value &rand) { return BooleanV(rand->v_type == V_BOOL); }
Value IsFixnum::evalRator(const Value &rand) { return BooleanV(rand->v_type == V_INT); }
Value IsNull::evalRator(const Value &rand) { return BooleanV(rand->v_type == V_NULL); }
Value IsPair::evalRator(const Value &rand) { return BooleanV(rand->v_type == V_PAIR); }
Value IsProcedure::evalRator(const Value &rand) { return BooleanV(rand->v_type == V_PROC); }
Value IsSymbol::evalRator(const Value &rand) { return BooleanV(rand->v_type == V_SYM); }
Value IsString::evalRator(const Value &rand) { return BooleanV(rand->v_type == V_STRING); }

Value Begin::eval(Assoc &e) {
    if (es.empty()) return VoidV();
    Value res = VoidV();
    for (auto &expr : es) {
        res = expr->eval(e);
    }
    return res;
}

static Value quoteSyntaxToValue(Syntax stx) {
    if (Number* n = dynamic_cast<Number*>(stx.get())) {
        return IntegerV(n->n);
    } else if (RationalSyntax* r = dynamic_cast<RationalSyntax*>(stx.get())) {
        return RationalV(r->numerator, r->denominator);
    } else if (TrueSyntax* t = dynamic_cast<TrueSyntax*>(stx.get())) {
        return BooleanV(true);
    } else if (FalseSyntax* f = dynamic_cast<FalseSyntax*>(stx.get())) {
        return BooleanV(false);
    } else if (SymbolSyntax* s = dynamic_cast<SymbolSyntax*>(stx.get())) {
        return SymbolV(s->s);
    } else if (StringSyntax* str = dynamic_cast<StringSyntax*>(stx.get())) {
        return StringV(str->s);
    } else if (List* l = dynamic_cast<List*>(stx.get())) {
        if (l->stxs.empty()) return NullV();
        Value res = NullV();
        bool has_dot = false;
        for (size_t i = 0; i < l->stxs.size(); ++i) {
            SymbolSyntax* sym = dynamic_cast<SymbolSyntax*>(l->stxs[i].get());
            if (sym && sym->s == ".") {
                has_dot = true;
                if (i != l->stxs.size() - 2) throw RuntimeError("Invalid dot in list");
            }
        }
        if (has_dot) {
            res = quoteSyntaxToValue(l->stxs.back());
            for (int i = (int)l->stxs.size() - 3; i >= 0; --i) {
                res = PairV(quoteSyntaxToValue(l->stxs[i]), res);
            }
        } else {
            for (auto it = l->stxs.rbegin(); it != l->stxs.rend(); ++it) {
                res = PairV(quoteSyntaxToValue(*it), res);
            }
        }
        return res;
    }
    throw RuntimeError("Unknown syntax in quote");
}

Value Quote::eval(Assoc& e) {
    return quoteSyntaxToValue(s);
}

Value AndVar::eval(Assoc &e) { // and with short-circuit evaluation
    Value res = BooleanV(true);
    for (auto &expr : rands) {
        res = expr->eval(e);
        if (res->v_type == V_BOOL && !dynamic_cast<Boolean*>(res.get())->b) {
            return BooleanV(false);
        }
    }
    return res;
}

Value OrVar::eval(Assoc &e) { // or with short-circuit evaluation
    Value res = BooleanV(false);
    for (auto &expr : rands) {
        res = expr->eval(e);
        if (!(res->v_type == V_BOOL && !dynamic_cast<Boolean*>(res.get())->b)) {
            return res;
        }
    }
    return res;
}

Value Not::evalRator(const Value &rand) { // not
    if (rand->v_type == V_BOOL && !dynamic_cast<Boolean*>(rand.get())->b) {
        return BooleanV(true);
    }
    return BooleanV(false);
}

Value If::eval(Assoc &e) {
    Value c = cond->eval(e);
    if (c->v_type == V_BOOL && !dynamic_cast<Boolean*>(c.get())->b) {
        return alter->eval(e);
    } else {
        return conseq->eval(e);
    }
}

Value Cond::eval(Assoc &env) {
    for (auto &clause : clauses) {
        if (clause.empty()) continue;
        Value c = clause[0]->eval(env);
        if (!(c->v_type == V_BOOL && !dynamic_cast<Boolean*>(c.get())->b)) {
            if (clause.size() == 1) return c;
            Value res = VoidV();
            for (size_t i = 1; i < clause.size(); ++i) {
                res = clause[i]->eval(env);
            }
            return res;
        }
    }
    return VoidV();
}

Value Lambda::eval(Assoc &env) { 
    return ProcedureV(x, e, env);
}

Value Apply::eval(Assoc &e) {
    Value op = rator->eval(e);
    if (op->v_type != V_PROC) {throw RuntimeError("Attempt to apply a non-procedure");}

    Procedure* clos_ptr = dynamic_cast<Procedure*>(op.get());
    
    std::vector<Value> args;
    for (auto &expr : rand) {
        args.push_back(expr->eval(e));
    }
    
    if (auto varNode = dynamic_cast<Variadic*>(clos_ptr->e.get())) {
        return varNode->evalRator(args);
    }
    if (args.size() != clos_ptr->parameters.size()) throw RuntimeError("Wrong number of arguments");
    
    Assoc param_env = clos_ptr->env;
    for (size_t i = 0; i < args.size(); ++i) {
        param_env = extend(clos_ptr->parameters[i], args[i], param_env);
    }
    return clos_ptr->e->eval(param_env);
}

Value Define::eval(Assoc &env) {
    Value existing = find(var, env);
    if (existing.get() != nullptr) {
        Value evaluated = e->eval(env);
        modify(var, evaluated, env);
    } else {
        env = extend(var, VoidV(), env);
        Value evaluated = e->eval(env);
        modify(var, evaluated, env);
    }
    return VoidV();
}

Value Let::eval(Assoc &env) {
    Assoc new_env = env;
    std::vector<Value> eval_vals;
    for (auto &b : bind) {
        eval_vals.push_back(b.second->eval(env));
    }
    for (size_t i = 0; i < bind.size(); ++i) {
        new_env = extend(bind[i].first, eval_vals[i], new_env);
    }
    return body->eval(new_env);
}

Value Letrec::eval(Assoc &env) {
    Assoc new_env = env;
    for (auto &b : bind) {
        new_env = extend(b.first, VoidV(), new_env);
    }
    std::vector<Value> eval_vals;
    for (auto &b : bind) {
        eval_vals.push_back(b.second->eval(new_env));
    }
    for (size_t i = 0; i < bind.size(); ++i) {
        modify(bind[i].first, eval_vals[i], new_env);
    }
    return body->eval(new_env);
}

Value Set::eval(Assoc &env) {
    Value existing = find(var, env);
    if (existing.get() == nullptr) {
        throw RuntimeError("set! undefined variable");
    }
    Value evaluated = e->eval(env);
    modify(var, evaluated, env);
    return VoidV();
}

Value Display::evalRator(const Value &rand) { // display function
    if (rand->v_type == V_STRING) {
        String* str_ptr = dynamic_cast<String*>(rand.get());
        std::cout << str_ptr->s;
    } else {
        rand->show(std::cout);
    }
    return VoidV();
}
