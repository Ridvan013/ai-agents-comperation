#include "RE.hpp"
#include "Def.hpp"
#include "syntax.hpp"
#include "value.hpp"
#include "expr.hpp"

#include <map>
#include <string>
#include <vector>

using std::pair;
using std::string;
using std::vector;

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

namespace {

Expr makeBodyExpr(const vector<Expr> &exprs) {
    if (exprs.empty()) {
        return Expr(new MakeVoid());
    }
    if (exprs.size() == 1) {
        return exprs[0];
    }
    return Expr(new Begin(exprs));
}

vector<Expr> parseExprList(const vector<Syntax> &items, size_t start, Assoc &env) {
    vector<Expr> parsed;
    for (size_t i = start; i < items.size(); ++i) {
        parsed.push_back(items[i]->parse(env));
    }
    return parsed;
}

Assoc extendParseEnv(const vector<string> &names, Assoc env) {
    for (const auto &name : names) {
        env = extend(name, VoidV(), env);
    }
    return env;
}

vector<string> parseParameterNames(const Syntax &syntax) {
    auto list = dynamic_cast<List *>(syntax.get());
    if (list == nullptr) {
        throw RuntimeError("Lambda parameters must be a list");
    }

    vector<string> names;
    for (const auto &item : list->stxs) {
        auto symbol = dynamic_cast<SymbolSyntax *>(item.get());
        if (symbol == nullptr) {
            throw RuntimeError("Parameter name must be a symbol");
        }
        names.push_back(symbol->s);
    }
    return names;
}

vector<pair<string, Expr>> parseBindings(const Syntax &syntax, Assoc &value_env) {
    auto list = dynamic_cast<List *>(syntax.get());
    if (list == nullptr) {
        throw RuntimeError("Bindings must be a list");
    }

    vector<pair<string, Expr>> bindings;
    for (const auto &binding_syntax : list->stxs) {
        auto binding_list = dynamic_cast<List *>(binding_syntax.get());
        if (binding_list == nullptr || binding_list->stxs.size() != 2) {
            throw RuntimeError("Invalid binding form");
        }

        auto symbol = dynamic_cast<SymbolSyntax *>(binding_list->stxs[0].get());
        if (symbol == nullptr) {
            throw RuntimeError("Binding name must be a symbol");
        }

        bindings.push_back({symbol->s, binding_list->stxs[1]->parse(value_env)});
    }
    return bindings;
}

Expr parseApplyLikeList(const vector<Syntax> &items, Assoc &env) {
    vector<Expr> args = parseExprList(items, 1, env);
    return Expr(new Apply(items[0]->parse(env), args));
}

} // namespace

Expr Syntax::parse(Assoc &env) {
    throw RuntimeError("Unimplemented parse method");
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
    if (stxs.empty()) {
        return Expr(new Quote(Syntax(new List())));
    }

    SymbolSyntax *id = dynamic_cast<SymbolSyntax *>(stxs[0].get());
    if (id == nullptr) {
        return parseApplyLikeList(stxs, env);
    }

    string op = id->s;
    if (find(op, env).get() != nullptr) {
        vector<Expr> args = parseExprList(stxs, 1, env);
        return Expr(new Apply(Expr(new Var(op)), args));
    }

    if (primitives.count(op) != 0) {
        vector<Expr> parameters = parseExprList(stxs, 1, env);
        ExprType op_type = primitives[op];

        switch (op_type) {
            case E_PLUS: return Expr(new PlusVar(parameters));
            case E_MINUS: return Expr(new MinusVar(parameters));
            case E_MUL: return Expr(new MultVar(parameters));
            case E_DIV: return Expr(new DivVar(parameters));
            case E_MODULO:
                if (parameters.size() != 2) {
                    throw RuntimeError("Wrong number of arguments for modulo");
                }
                return Expr(new Modulo(parameters[0], parameters[1]));
            case E_EXPT:
                if (parameters.size() != 2) {
                    throw RuntimeError("Wrong number of arguments for expt");
                }
                return Expr(new Expt(parameters[0], parameters[1]));
            case E_LT: return Expr(new LessVar(parameters));
            case E_LE: return Expr(new LessEqVar(parameters));
            case E_EQ: return Expr(new EqualVar(parameters));
            case E_GE: return Expr(new GreaterEqVar(parameters));
            case E_GT: return Expr(new GreaterVar(parameters));
            case E_CONS:
                if (parameters.size() != 2) {
                    throw RuntimeError("Wrong number of arguments for cons");
                }
                return Expr(new Cons(parameters[0], parameters[1]));
            case E_CAR:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for car");
                }
                return Expr(new Car(parameters[0]));
            case E_CDR:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for cdr");
                }
                return Expr(new Cdr(parameters[0]));
            case E_LIST:
                return Expr(new ListFunc(parameters));
            case E_SETCAR:
                if (parameters.size() != 2) {
                    throw RuntimeError("Wrong number of arguments for set-car!");
                }
                return Expr(new SetCar(parameters[0], parameters[1]));
            case E_SETCDR:
                if (parameters.size() != 2) {
                    throw RuntimeError("Wrong number of arguments for set-cdr!");
                }
                return Expr(new SetCdr(parameters[0], parameters[1]));
            case E_NOT:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for not");
                }
                return Expr(new Not(parameters[0]));
            case E_AND:
                return Expr(new AndVar(parameters));
            case E_OR:
                return Expr(new OrVar(parameters));
            case E_EQQ:
                if (parameters.size() != 2) {
                    throw RuntimeError("Wrong number of arguments for eq?");
                }
                return Expr(new IsEq(parameters[0], parameters[1]));
            case E_BOOLQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for boolean?");
                }
                return Expr(new IsBoolean(parameters[0]));
            case E_INTQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for number?");
                }
                return Expr(new IsFixnum(parameters[0]));
            case E_NULLQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for null?");
                }
                return Expr(new IsNull(parameters[0]));
            case E_PAIRQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for pair?");
                }
                return Expr(new IsPair(parameters[0]));
            case E_PROCQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for procedure?");
                }
                return Expr(new IsProcedure(parameters[0]));
            case E_SYMBOLQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for symbol?");
                }
                return Expr(new IsSymbol(parameters[0]));
            case E_LISTQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for list?");
                }
                return Expr(new IsList(parameters[0]));
            case E_STRINGQ:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for string?");
                }
                return Expr(new IsString(parameters[0]));
            case E_DISPLAY:
                if (parameters.size() != 1) {
                    throw RuntimeError("Wrong number of arguments for display");
                }
                return Expr(new Display(parameters[0]));
            case E_VOID:
                if (!parameters.empty()) {
                    throw RuntimeError("Wrong number of arguments for void");
                }
                return Expr(new MakeVoid());
            case E_EXIT:
                if (!parameters.empty()) {
                    throw RuntimeError("Wrong number of arguments for exit");
                }
                return Expr(new Exit());
            default:
                throw RuntimeError("Unknown primitive");
        }
    }

    if (reserved_words.count(op) != 0) {
        switch (reserved_words[op]) {
            case E_BEGIN: {
                vector<Expr> exprs = parseExprList(stxs, 1, env);
                return Expr(new Begin(exprs));
            }
            case E_QUOTE:
                if (stxs.size() != 2) {
                    throw RuntimeError("quote expects exactly one argument");
                }
                return Expr(new Quote(stxs[1]));
            case E_IF:
                if (stxs.size() != 4) {
                    throw RuntimeError("if expects exactly three arguments");
                }
                return Expr(new If(stxs[1]->parse(env), stxs[2]->parse(env), stxs[3]->parse(env)));
            case E_COND: {
                vector<vector<Expr>> clauses;
                for (size_t i = 1; i < stxs.size(); ++i) {
                    auto clause = dynamic_cast<List *>(stxs[i].get());
                    if (clause == nullptr || clause->stxs.empty()) {
                        throw RuntimeError("Invalid cond clause");
                    }

                    vector<Expr> parsed_clause;
                    for (const auto &item : clause->stxs) {
                        parsed_clause.push_back(item->parse(env));
                    }
                    clauses.push_back(parsed_clause);
                }
                return Expr(new Cond(clauses));
            }
            case E_LAMBDA: {
                if (stxs.size() < 3) {
                    throw RuntimeError("lambda requires parameters and body");
                }
                vector<string> params = parseParameterNames(stxs[1]);
                Assoc body_env = extendParseEnv(params, env);
                vector<Expr> body = parseExprList(stxs, 2, body_env);
                return Expr(new Lambda(params, makeBodyExpr(body)));
            }
            case E_DEFINE: {
                if (stxs.size() < 3) {
                    throw RuntimeError("define requires a name and value");
                }

                auto name_symbol = dynamic_cast<SymbolSyntax *>(stxs[1].get());
                if (name_symbol != nullptr) {
                    if (stxs.size() != 3) {
                        throw RuntimeError("define variable form expects one value expression");
                    }
                    return Expr(new Define(name_symbol->s, stxs[2]->parse(env)));
                }

                auto header = dynamic_cast<List *>(stxs[1].get());
                if (header == nullptr || header->stxs.empty()) {
                    throw RuntimeError("Invalid function definition");
                }

                auto func_name = dynamic_cast<SymbolSyntax *>(header->stxs[0].get());
                if (func_name == nullptr) {
                    throw RuntimeError("Function name must be a symbol");
                }

                vector<string> params;
                for (size_t i = 1; i < header->stxs.size(); ++i) {
                    auto param = dynamic_cast<SymbolSyntax *>(header->stxs[i].get());
                    if (param == nullptr) {
                        throw RuntimeError("Function parameter must be a symbol");
                    }
                    params.push_back(param->s);
                }

                Assoc body_env = extendParseEnv(params, env);
                vector<Expr> body = parseExprList(stxs, 2, body_env);
                return Expr(new Define(func_name->s, Expr(new Lambda(params, makeBodyExpr(body)))));
            }
            case E_LET:
            case E_LETREC: {
                if (stxs.size() < 3) {
                    throw RuntimeError("let/letrec requires bindings and body");
                }

                auto binding_list = dynamic_cast<List *>(stxs[1].get());
                if (binding_list == nullptr) {
                    throw RuntimeError("Bindings must be a list");
                }

                vector<string> names;
                for (const auto &binding_syntax : binding_list->stxs) {
                    auto binding = dynamic_cast<List *>(binding_syntax.get());
                    if (binding == nullptr || binding->stxs.size() != 2) {
                        throw RuntimeError("Invalid binding form");
                    }
                    auto symbol = dynamic_cast<SymbolSyntax *>(binding->stxs[0].get());
                    if (symbol == nullptr) {
                        throw RuntimeError("Binding name must be a symbol");
                    }
                    names.push_back(symbol->s);
                }

                Assoc rhs_env = env;
                if (reserved_words[op] == E_LETREC) {
                    rhs_env = extendParseEnv(names, env);
                }
                vector<pair<string, Expr>> bindings = parseBindings(stxs[1], rhs_env);
                Assoc body_env = extendParseEnv(names, env);
                vector<Expr> body = parseExprList(stxs, 2, body_env);

                if (reserved_words[op] == E_LET) {
                    return Expr(new Let(bindings, makeBodyExpr(body)));
                }
                return Expr(new Letrec(bindings, makeBodyExpr(body)));
            }
            case E_SET: {
                if (stxs.size() != 3) {
                    throw RuntimeError("set! expects exactly two arguments");
                }
                auto symbol = dynamic_cast<SymbolSyntax *>(stxs[1].get());
                if (symbol == nullptr) {
                    throw RuntimeError("set! target must be a symbol");
                }
                return Expr(new Set(symbol->s, stxs[2]->parse(env)));
            }
            default:
                throw RuntimeError("Unknown reserved word: " + op);
        }
    }

    vector<Expr> args = parseExprList(stxs, 1, env);
    return Expr(new Apply(Expr(new Var(op)), args));
}
