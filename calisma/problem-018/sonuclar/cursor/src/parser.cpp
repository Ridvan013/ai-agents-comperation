/**
 * @file parser.cpp
 * @brief Parsing implementation for Scheme syntax tree to expression tree conversion
 *
 * This file implements the parsing logic that converts syntax trees into
 * expression trees that can be evaluated. It handles literals, special forms,
 * primitive operations, and function applications.
 *
 * Key idea: the parser threads a lexical environment (`env`). Whenever a name
 * is locally bound (lambda/let/letrec parameters, define names), it is inserted
 * into this environment with a placeholder value. When the head of a list is a
 * symbol that is already bound in this environment, the list is parsed as a
 * function application (Apply) rather than as a primitive/special form. This is
 * what allows reserved words and primitives (like `+`, `begin`, `lambda`) to be
 * shadowed by ordinary variables, as required by the specification.
 */

#include "RE.hpp"
#include "Def.hpp"
#include "syntax.hpp"
#include "value.hpp"
#include "expr.hpp"
#include <map>
#include <string>
#include <iostream>

#define mp make_pair
using std::string;
using std::vector;
using std::pair;

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

// A placeholder value used to mark a name as "lexically bound" during parsing.
static Value parsePlaceholder() { return SymbolV("@parse"); }

// Extend a (copied) environment with a bound name for parse-time scope tracking.
static Assoc parseBind(const string &name, Assoc env) {
    return extend(name, parsePlaceholder(), env);
}

// Parse every syntax element in [from, end) as an expression.
static vector<Expr> parseArgs(vector<Syntax> &stxs, size_t from, Assoc &env) {
    vector<Expr> out;
    for (size_t i = from; i < stxs.size(); ++i)
        out.push_back(stxs[i].parse(env));
    return out;
}

// Parse a body consisting of one or more expressions (index [from, end)).
// If there is a single expression it is returned directly, otherwise the
// expressions are wrapped in a Begin so that they are evaluated in order.
static Expr parseBody(vector<Syntax> &stxs, size_t from, Assoc &env) {
    vector<Expr> body = parseArgs(stxs, from, env);
    if (body.empty())
        return Expr(new MakeVoid());
    if (body.size() == 1)
        return body[0];
    return Expr(new Begin(body));
}

// Extract a list of parameter names from a syntax node representing (a b c ...).
static vector<string> extractParams(Syntax s) {
    List *l = dynamic_cast<List *>(s.get());
    if (l == nullptr)
        throw RuntimeError("Expected a parameter list");
    vector<string> params;
    for (auto &item : l->stxs) {
        SymbolSyntax *sym = dynamic_cast<SymbolSyntax *>(item.get());
        if (sym == nullptr)
            throw RuntimeError("Parameter names must be symbols");
        params.push_back(sym->s);
    }
    return params;
}

static void requireArity(const vector<Expr> &v, size_t n, const string &name) {
    if (v.size() != n)
        throw RuntimeError("Wrong number of arguments for " + name);
}

/**
 * @brief Syntax wrapper: delegate to the underlying node's virtual parse method.
 */
Expr Syntax::parse(Assoc &env) {
    return ptr->parse(env);
}

Expr Number::parse(Assoc &env) {
    return Expr(new Fixnum(n));
}

Expr RationalSyntax::parse(Assoc &env) {
    return Expr(new RationalNum(numerator, denominator));
}

Expr SymbolSyntax::parse(Assoc &env) {
    return Expr(new Var(s));
}

Expr StringSyntax::parse(Assoc &env) {
    return Expr(new StringExpr(s));
}

Expr TrueSyntax::parse(Assoc &env) {
    return Expr(new True());
}

Expr FalseSyntax::parse(Assoc &env) {
    return Expr(new False());
}

// Parse a special form (reserved word). `op` is guaranteed to be a reserved
// word that is NOT shadowed by a local binding.
static Expr parseReserved(const string &op, vector<Syntax> &stxs, Assoc &env) {
    switch (reserved_words[op]) {
        case E_QUOTE: {
            if (stxs.size() != 2)
                throw RuntimeError("quote expects exactly one argument");
            return Expr(new Quote(stxs[1]));
        }
        case E_BEGIN: {
            return Expr(new Begin(parseArgs(stxs, 1, env)));
        }
        case E_IF: {
            if (stxs.size() != 3 && stxs.size() != 4)
                throw RuntimeError("if expects 2 or 3 arguments");
            Expr c = stxs[1].parse(env);
            Expr t = stxs[2].parse(env);
            Expr e = (stxs.size() == 4) ? stxs[3].parse(env)
                                        : Expr(new MakeVoid());
            return Expr(new If(c, t, e));
        }
        case E_COND: {
            vector<vector<Expr>> clauses;
            for (size_t i = 1; i < stxs.size(); ++i) {
                List *clause = dynamic_cast<List *>(stxs[i].get());
                if (clause == nullptr || clause->stxs.empty())
                    throw RuntimeError("Malformed cond clause");
                vector<Expr> parsed;
                SymbolSyntax *head = dynamic_cast<SymbolSyntax *>(clause->stxs[0].get());
                bool isElse = (head != nullptr && head->s == "else" &&
                               find("else", env).get() == nullptr);
                if (isElse)
                    parsed.push_back(Expr(new True()));
                else
                    parsed.push_back(clause->stxs[0].parse(env));
                for (size_t j = 1; j < clause->stxs.size(); ++j)
                    parsed.push_back(clause->stxs[j].parse(env));
                clauses.push_back(parsed);
            }
            return Expr(new Cond(clauses));
        }
        case E_LAMBDA: {
            if (stxs.size() < 3)
                throw RuntimeError("lambda expects parameters and a body");
            vector<string> params = extractParams(stxs[1]);
            Assoc bodyEnv = env;
            for (auto &p : params)
                bodyEnv = parseBind(p, bodyEnv);
            Expr body = parseBody(stxs, 2, bodyEnv);
            return Expr(new Lambda(params, body));
        }
        case E_DEFINE: {
            if (stxs.size() < 3)
                throw RuntimeError("define expects a name and a value");
            // Form 2: (define (f args...) body...)
            if (List *sig = dynamic_cast<List *>(stxs[1].get())) {
                if (sig->stxs.empty())
                    throw RuntimeError("Malformed define");
                SymbolSyntax *fn = dynamic_cast<SymbolSyntax *>(sig->stxs[0].get());
                if (fn == nullptr)
                    throw RuntimeError("Function name must be a symbol");
                string name = fn->s;
                vector<string> params;
                for (size_t i = 1; i < sig->stxs.size(); ++i) {
                    SymbolSyntax *p = dynamic_cast<SymbolSyntax *>(sig->stxs[i].get());
                    if (p == nullptr)
                        throw RuntimeError("Parameter names must be symbols");
                    params.push_back(p->s);
                }
                Assoc bodyEnv = parseBind(name, env);
                for (auto &p : params)
                    bodyEnv = parseBind(p, bodyEnv);
                Expr body = parseBody(stxs, 2, bodyEnv);
                return Expr(new Define(name, Expr(new Lambda(params, body))));
            }
            // Form 1: (define var value...)
            SymbolSyntax *sym = dynamic_cast<SymbolSyntax *>(stxs[1].get());
            if (sym == nullptr)
                throw RuntimeError("define target must be a symbol");
            Assoc valEnv = parseBind(sym->s, env);
            Expr value = parseBody(stxs, 2, valEnv);
            return Expr(new Define(sym->s, value));
        }
        case E_LET: {
            if (stxs.size() < 3)
                throw RuntimeError("Malformed let");
            List *binds = dynamic_cast<List *>(stxs[1].get());
            if (binds == nullptr)
                throw RuntimeError("Malformed let bindings");
            vector<pair<string, Expr>> bindings;
            Assoc bodyEnv = env;
            for (auto &b : binds->stxs) {
                List *one = dynamic_cast<List *>(b.get());
                if (one == nullptr || one->stxs.size() != 2)
                    throw RuntimeError("Malformed let binding");
                SymbolSyntax *name = dynamic_cast<SymbolSyntax *>(one->stxs[0].get());
                if (name == nullptr)
                    throw RuntimeError("let binding name must be a symbol");
                // Value expressions are parsed in the OUTER environment.
                Expr value = one->stxs[1].parse(env);
                bindings.push_back(mp(name->s, value));
                bodyEnv = parseBind(name->s, bodyEnv);
            }
            Expr body = parseBody(stxs, 2, bodyEnv);
            return Expr(new Let(bindings, body));
        }
        case E_LETREC: {
            if (stxs.size() < 3)
                throw RuntimeError("Malformed letrec");
            List *binds = dynamic_cast<List *>(stxs[1].get());
            if (binds == nullptr)
                throw RuntimeError("Malformed letrec bindings");
            // All names are in scope for both the value expressions and body.
            Assoc innerEnv = env;
            vector<string> names;
            vector<Syntax> valueStx;
            for (auto &b : binds->stxs) {
                List *one = dynamic_cast<List *>(b.get());
                if (one == nullptr || one->stxs.size() != 2)
                    throw RuntimeError("Malformed letrec binding");
                SymbolSyntax *name = dynamic_cast<SymbolSyntax *>(one->stxs[0].get());
                if (name == nullptr)
                    throw RuntimeError("letrec binding name must be a symbol");
                names.push_back(name->s);
                valueStx.push_back(one->stxs[1]);
                innerEnv = parseBind(name->s, innerEnv);
            }
            vector<pair<string, Expr>> bindings;
            for (size_t i = 0; i < names.size(); ++i)
                bindings.push_back(mp(names[i], valueStx[i].parse(innerEnv)));
            Expr body = parseBody(stxs, 2, innerEnv);
            return Expr(new Letrec(bindings, body));
        }
        case E_SET: {
            if (stxs.size() != 3)
                throw RuntimeError("set! expects a variable and a value");
            SymbolSyntax *sym = dynamic_cast<SymbolSyntax *>(stxs[1].get());
            if (sym == nullptr)
                throw RuntimeError("set! target must be a symbol");
            return Expr(new Set(sym->s, stxs[2].parse(env)));
        }
        default:
            throw RuntimeError("Unknown reserved word: " + op);
    }
}

// Parse a primitive application. All arguments are evaluated (applicative order),
// so they are parsed as ordinary expressions.
static Expr parsePrimitive(const string &op, vector<Syntax> &stxs, Assoc &env) {
    vector<Expr> args = parseArgs(stxs, 1, env);
    switch (primitives[op]) {
        // Arithmetic (variadic)
        case E_PLUS:  return Expr(new PlusVar(args));
        case E_MINUS: return Expr(new MinusVar(args));
        case E_MUL:   return Expr(new MultVar(args));
        case E_DIV:   return Expr(new DivVar(args));
        case E_MODULO:
            requireArity(args, 2, "modulo");
            return Expr(new Modulo(args[0], args[1]));
        case E_EXPT:
            requireArity(args, 2, "expt");
            return Expr(new Expt(args[0], args[1]));

        // Comparison (variadic)
        case E_LT: return Expr(new LessVar(args));
        case E_LE: return Expr(new LessEqVar(args));
        case E_EQ: return Expr(new EqualVar(args));
        case E_GE: return Expr(new GreaterEqVar(args));
        case E_GT: return Expr(new GreaterVar(args));

        // List operations
        case E_CONS:
            requireArity(args, 2, "cons");
            return Expr(new Cons(args[0], args[1]));
        case E_CAR:
            requireArity(args, 1, "car");
            return Expr(new Car(args[0]));
        case E_CDR:
            requireArity(args, 1, "cdr");
            return Expr(new Cdr(args[0]));
        case E_LIST:
            return Expr(new ListFunc(args));
        case E_SETCAR:
            requireArity(args, 2, "set-car!");
            return Expr(new SetCar(args[0], args[1]));
        case E_SETCDR:
            requireArity(args, 2, "set-cdr!");
            return Expr(new SetCdr(args[0], args[1]));

        // Logic
        case E_NOT:
            requireArity(args, 1, "not");
            return Expr(new Not(args[0]));
        case E_AND: return Expr(new AndVar(args));
        case E_OR:  return Expr(new OrVar(args));

        // Type predicates
        case E_EQQ:
            requireArity(args, 2, "eq?");
            return Expr(new IsEq(args[0], args[1]));
        case E_BOOLQ:
            requireArity(args, 1, "boolean?");
            return Expr(new IsBoolean(args[0]));
        case E_INTQ:
            requireArity(args, 1, "number?");
            return Expr(new IsFixnum(args[0]));
        case E_NULLQ:
            requireArity(args, 1, "null?");
            return Expr(new IsNull(args[0]));
        case E_PAIRQ:
            requireArity(args, 1, "pair?");
            return Expr(new IsPair(args[0]));
        case E_PROCQ:
            requireArity(args, 1, "procedure?");
            return Expr(new IsProcedure(args[0]));
        case E_SYMBOLQ:
            requireArity(args, 1, "symbol?");
            return Expr(new IsSymbol(args[0]));
        case E_LISTQ:
            requireArity(args, 1, "list?");
            return Expr(new IsList(args[0]));
        case E_STRINGQ:
            requireArity(args, 1, "string?");
            return Expr(new IsString(args[0]));

        // I/O and control
        case E_DISPLAY:
            requireArity(args, 1, "display");
            return Expr(new Display(args[0]));
        case E_VOID:
            requireArity(args, 0, "void");
            return Expr(new MakeVoid());
        case E_EXIT:
            return Expr(new Exit());

        default:
            throw RuntimeError("Unknown primitive: " + op);
    }
}

Expr List::parse(Assoc &env) {
    if (stxs.empty()) {
        // The empty list is treated as the quoted empty list '().
        return Expr(new Quote(Syntax(new List())));
    }

    SymbolSyntax *id = dynamic_cast<SymbolSyntax *>(stxs[0].get());

    // Head is not a symbol -> evaluate it and apply (e.g. ((lambda ...) ...)).
    if (id == nullptr) {
        Expr rator = stxs[0].parse(env);
        return Expr(new Apply(rator, parseArgs(stxs, 1, env)));
    }

    string op = id->s;

    // A locally bound name always shadows primitives and special forms.
    if (find(op, env).get() != nullptr) {
        return Expr(new Apply(Expr(new Var(op)), parseArgs(stxs, 1, env)));
    }

    if (reserved_words.count(op) != 0)
        return parseReserved(op, stxs, env);

    if (primitives.count(op) != 0)
        return parsePrimitive(op, stxs, env);

    // Default: application of a (global/user) variable.
    return Expr(new Apply(Expr(new Var(op)), parseArgs(stxs, 1, env)));
}
