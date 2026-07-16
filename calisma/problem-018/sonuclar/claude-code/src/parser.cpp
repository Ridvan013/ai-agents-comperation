/**
 * @file parser.cpp
 * @brief Parsing implementation for Scheme syntax tree to expression tree conversion
 *
 * This file implements the parsing logic that converts syntax trees into
 * expression trees that can be evaluated.
 * primitive operations, and function applications.
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

/**
 * @brief Default parse method (should be overridden by subclasses)
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

Expr List::parse(Assoc &env) {
    // Empty list literal '()
    if (stxs.empty()) {
        return Expr(new Quote(Syntax(new List())));
    }

    // Helper: build a function-application expression.
    auto makeApply = [&]() -> Expr {
        Expr rator = stxs[0]->parse(env);
        vector<Expr> rands;
        for (size_t i = 1; i < stxs.size(); i++)
            rands.push_back(stxs[i]->parse(env));
        return Expr(new Apply(rator, rands));
    };

    // If the head is not a symbol, it must be an expression we apply.
    SymbolSyntax *id = dynamic_cast<SymbolSyntax *>(stxs[0].get());
    if (id == nullptr) {
        return makeApply();
    }

    string op = id->s;

    // If the operator name is bound as a (lexical / global) variable, the
    // primitive / reserved meaning is shadowed: treat it as an application.
    if (find(op, env).get() != nullptr) {
        return makeApply();
    }

    // Special forms (reserved words).
    if (reserved_words.count(op) != 0) {
        switch (reserved_words[op]) {
            case E_BEGIN: {
                vector<Expr> es;
                for (size_t i = 1; i < stxs.size(); i++)
                    es.push_back(stxs[i]->parse(env));
                return Expr(new Begin(es));
            }
            case E_QUOTE: {
                if (stxs.size() != 2)
                    throw RuntimeError("quote: bad syntax");
                return Expr(new Quote(stxs[1]));
            }
            case E_IF: {
                if (stxs.size() < 3)
                    throw RuntimeError("if: bad syntax");
                Expr c = stxs[1]->parse(env);
                Expr t = stxs[2]->parse(env);
                Expr a = (stxs.size() > 3) ? stxs[3]->parse(env)
                                           : Expr(new MakeVoid());
                return Expr(new If(c, t, a));
            }
            case E_COND: {
                vector<vector<Expr>> clauses;
                for (size_t i = 1; i < stxs.size(); i++) {
                    List *cl = dynamic_cast<List *>(stxs[i].get());
                    if (cl == nullptr || cl->stxs.empty())
                        throw RuntimeError("cond: bad clause");
                    vector<Expr> clause;
                    SymbolSyntax *sym =
                        dynamic_cast<SymbolSyntax *>(cl->stxs[0].get());
                    if (sym != nullptr && sym->s == "else")
                        clause.push_back(Expr(new True()));
                    else
                        clause.push_back(cl->stxs[0]->parse(env));
                    for (size_t j = 1; j < cl->stxs.size(); j++)
                        clause.push_back(cl->stxs[j]->parse(env));
                    clauses.push_back(clause);
                }
                return Expr(new Cond(clauses));
            }
            case E_LAMBDA: {
                if (stxs.size() < 3)
                    throw RuntimeError("lambda: bad syntax");
                List *plist = dynamic_cast<List *>(stxs[1].get());
                if (plist == nullptr)
                    throw RuntimeError("lambda: bad parameter list");
                vector<string> params;
                for (auto &ps : plist->stxs) {
                    SymbolSyntax *p = dynamic_cast<SymbolSyntax *>(ps.get());
                    if (p == nullptr)
                        throw RuntimeError("lambda: bad parameter");
                    params.push_back(p->s);
                }
                Assoc bodyEnv = env;
                for (auto &p : params)
                    bodyEnv = extend(p, VoidV(), bodyEnv);
                vector<Expr> body;
                for (size_t i = 2; i < stxs.size(); i++)
                    body.push_back(stxs[i]->parse(bodyEnv));
                Expr bodyExpr =
                    (body.size() == 1) ? body[0] : Expr(new Begin(body));
                return Expr(new Lambda(params, bodyExpr));
            }
            case E_DEFINE: {
                if (stxs.size() < 3)
                    throw RuntimeError("define: bad syntax");
                // (define var expr)
                if (SymbolSyntax *sym =
                        dynamic_cast<SymbolSyntax *>(stxs[1].get())) {
                    if (stxs.size() == 3) {
                        return Expr(new Define(sym->s, stxs[2]->parse(env)));
                    }
                    vector<Expr> body;
                    for (size_t i = 2; i < stxs.size(); i++)
                        body.push_back(stxs[i]->parse(env));
                    return Expr(new Define(sym->s, Expr(new Begin(body))));
                }
                // (define (f args...) body...)
                if (List *lst = dynamic_cast<List *>(stxs[1].get())) {
                    if (lst->stxs.empty())
                        throw RuntimeError("define: bad function form");
                    SymbolSyntax *fsym =
                        dynamic_cast<SymbolSyntax *>(lst->stxs[0].get());
                    if (fsym == nullptr)
                        throw RuntimeError("define: bad function name");
                    vector<string> params;
                    for (size_t i = 1; i < lst->stxs.size(); i++) {
                        SymbolSyntax *p =
                            dynamic_cast<SymbolSyntax *>(lst->stxs[i].get());
                        if (p == nullptr)
                            throw RuntimeError("define: bad parameter");
                        params.push_back(p->s);
                    }
                    Assoc bodyEnv = env;
                    for (auto &p : params)
                        bodyEnv = extend(p, VoidV(), bodyEnv);
                    vector<Expr> body;
                    for (size_t i = 2; i < stxs.size(); i++)
                        body.push_back(stxs[i]->parse(bodyEnv));
                    Expr bodyExpr =
                        (body.size() == 1) ? body[0] : Expr(new Begin(body));
                    Expr lam = Expr(new Lambda(params, bodyExpr));
                    return Expr(new Define(fsym->s, lam));
                }
                throw RuntimeError("define: bad syntax");
            }
            case E_LET: {
                if (stxs.size() < 3)
                    throw RuntimeError("let: bad syntax");
                List *blist = dynamic_cast<List *>(stxs[1].get());
                if (blist == nullptr)
                    throw RuntimeError("let: bad bindings");
                vector<pair<string, Expr>> binds;
                vector<string> names;
                for (auto &bs : blist->stxs) {
                    List *bl = dynamic_cast<List *>(bs.get());
                    if (bl == nullptr || bl->stxs.size() != 2)
                        throw RuntimeError("let: bad binding");
                    SymbolSyntax *nm =
                        dynamic_cast<SymbolSyntax *>(bl->stxs[0].get());
                    if (nm == nullptr)
                        throw RuntimeError("let: bad binding name");
                    // values are evaluated in the OUTER scope
                    Expr v = bl->stxs[1]->parse(env);
                    binds.push_back(mp(nm->s, v));
                    names.push_back(nm->s);
                }
                Assoc bodyEnv = env;
                for (auto &n : names)
                    bodyEnv = extend(n, VoidV(), bodyEnv);
                vector<Expr> body;
                for (size_t i = 2; i < stxs.size(); i++)
                    body.push_back(stxs[i]->parse(bodyEnv));
                Expr bodyExpr =
                    (body.size() == 1) ? body[0] : Expr(new Begin(body));
                return Expr(new Let(binds, bodyExpr));
            }
            case E_LETREC: {
                if (stxs.size() < 3)
                    throw RuntimeError("letrec: bad syntax");
                List *blist = dynamic_cast<List *>(stxs[1].get());
                if (blist == nullptr)
                    throw RuntimeError("letrec: bad bindings");
                vector<string> names;
                vector<Syntax> valStxs;
                for (auto &bs : blist->stxs) {
                    List *bl = dynamic_cast<List *>(bs.get());
                    if (bl == nullptr || bl->stxs.size() != 2)
                        throw RuntimeError("letrec: bad binding");
                    SymbolSyntax *nm =
                        dynamic_cast<SymbolSyntax *>(bl->stxs[0].get());
                    if (nm == nullptr)
                        throw RuntimeError("letrec: bad binding name");
                    names.push_back(nm->s);
                    valStxs.push_back(bl->stxs[1]);
                }
                // names are all in scope while evaluating the values
                Assoc bodyEnv = env;
                for (auto &n : names)
                    bodyEnv = extend(n, VoidV(), bodyEnv);
                vector<pair<string, Expr>> binds;
                for (size_t i = 0; i < names.size(); i++)
                    binds.push_back(mp(names[i], valStxs[i]->parse(bodyEnv)));
                vector<Expr> body;
                for (size_t i = 2; i < stxs.size(); i++)
                    body.push_back(stxs[i]->parse(bodyEnv));
                Expr bodyExpr =
                    (body.size() == 1) ? body[0] : Expr(new Begin(body));
                return Expr(new Letrec(binds, bodyExpr));
            }
            case E_SET: {
                if (stxs.size() != 3)
                    throw RuntimeError("set!: bad syntax");
                SymbolSyntax *nm =
                    dynamic_cast<SymbolSyntax *>(stxs[1].get());
                if (nm == nullptr)
                    throw RuntimeError("set!: bad variable");
                Expr v = stxs[2]->parse(env);
                return Expr(new Set(nm->s, v));
            }
            default:
                throw RuntimeError("Unknown reserved word: " + op);
        }
    }

    // Primitive operators.
    if (primitives.count(op) != 0) {
        vector<Expr> ps;
        for (size_t i = 1; i < stxs.size(); i++)
            ps.push_back(stxs[i]->parse(env));

        switch (primitives[op]) {
            case E_PLUS:  return Expr(new PlusVar(ps));
            case E_MINUS: return Expr(new MinusVar(ps));
            case E_MUL:   return Expr(new MultVar(ps));
            case E_DIV:   return Expr(new DivVar(ps));
            case E_MODULO:
                if (ps.size() != 2)
                    throw RuntimeError("Wrong number of arguments for modulo");
                return Expr(new Modulo(ps[0], ps[1]));
            case E_EXPT:
                if (ps.size() != 2)
                    throw RuntimeError("Wrong number of arguments for expt");
                return Expr(new Expt(ps[0], ps[1]));
            case E_LT: return Expr(new LessVar(ps));
            case E_LE: return Expr(new LessEqVar(ps));
            case E_EQ: return Expr(new EqualVar(ps));
            case E_GE: return Expr(new GreaterEqVar(ps));
            case E_GT: return Expr(new GreaterVar(ps));
            case E_CONS:
                if (ps.size() != 2)
                    throw RuntimeError("Wrong number of arguments for cons");
                return Expr(new Cons(ps[0], ps[1]));
            case E_CAR:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for car");
                return Expr(new Car(ps[0]));
            case E_CDR:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for cdr");
                return Expr(new Cdr(ps[0]));
            case E_LIST: return Expr(new ListFunc(ps));
            case E_SETCAR:
                if (ps.size() != 2)
                    throw RuntimeError("Wrong number of arguments for set-car!");
                return Expr(new SetCar(ps[0], ps[1]));
            case E_SETCDR:
                if (ps.size() != 2)
                    throw RuntimeError("Wrong number of arguments for set-cdr!");
                return Expr(new SetCdr(ps[0], ps[1]));
            case E_NOT:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for not");
                return Expr(new Not(ps[0]));
            case E_AND: return Expr(new AndVar(ps));
            case E_OR:  return Expr(new OrVar(ps));
            case E_EQQ:
                if (ps.size() != 2)
                    throw RuntimeError("Wrong number of arguments for eq?");
                return Expr(new IsEq(ps[0], ps[1]));
            case E_BOOLQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for boolean?");
                return Expr(new IsBoolean(ps[0]));
            case E_INTQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for number?");
                return Expr(new IsFixnum(ps[0]));
            case E_NULLQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for null?");
                return Expr(new IsNull(ps[0]));
            case E_PAIRQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for pair?");
                return Expr(new IsPair(ps[0]));
            case E_PROCQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for procedure?");
                return Expr(new IsProcedure(ps[0]));
            case E_SYMBOLQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for symbol?");
                return Expr(new IsSymbol(ps[0]));
            case E_LISTQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for list?");
                return Expr(new IsList(ps[0]));
            case E_STRINGQ:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for string?");
                return Expr(new IsString(ps[0]));
            case E_DISPLAY:
                if (ps.size() != 1)
                    throw RuntimeError("Wrong number of arguments for display");
                return Expr(new Display(ps[0]));
            case E_VOID:
                if (ps.size() != 0)
                    throw RuntimeError("Wrong number of arguments for void");
                return Expr(new MakeVoid());
            case E_EXIT:
                return Expr(new Exit());
            default:
                break;
        }
    }

    // default: function application on a (possibly undefined) variable
    return makeApply();
}
