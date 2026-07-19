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
#include <vector>

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

    SymbolSyntax *id = dynamic_cast<SymbolSyntax*>(stxs[0].get());
    if (id == nullptr) {
        vector<Expr> parameters;
        for (size_t i = 1; i < stxs.size(); ++i) parameters.push_back(stxs[i]->parse(env));
        return Expr(new Apply(stxs[0]->parse(env), parameters));
    } else {
        string op = id->s;
        
        if (find(op, env).get() != nullptr) {
            vector<Expr> parameters;
            for (size_t i = 1; i < stxs.size(); ++i) parameters.push_back(stxs[i]->parse(env));
            return Expr(new Apply(Expr(new Var(op)), parameters));
        }

        if (primitives.count(op) != 0) {
            vector<Expr> parameters;
            for (size_t i = 1; i < stxs.size(); ++i) {
                parameters.push_back(stxs[i]->parse(env));
            }
            
            ExprType op_type = primitives[op];
            if (op_type == E_PLUS) {
                if (parameters.size() == 2) return Expr(new Plus(parameters[0], parameters[1])); 
                else return Expr(new PlusVar(parameters));
            } else if (op_type == E_MINUS) {
                if (parameters.size() == 2) return Expr(new Minus(parameters[0], parameters[1]));
                else return Expr(new MinusVar(parameters));
            } else if (op_type == E_MUL) {
                if (parameters.size() == 2) return Expr(new Mult(parameters[0], parameters[1]));
                else return Expr(new MultVar(parameters));
            }  else if (op_type == E_DIV) {
                if (parameters.size() == 2) return Expr(new Div(parameters[0], parameters[1]));
                else return Expr(new DivVar(parameters));
            } else if (op_type == E_MODULO) {
                if (parameters.size() != 2) throw RuntimeError("Wrong number of arguments for modulo");
                return Expr(new Modulo(parameters[0], parameters[1]));
            } else if (op_type == E_EXPT) {
                if (parameters.size() != 2) throw RuntimeError("Wrong number of arguments for expt");
                return Expr(new Expt(parameters[0], parameters[1]));
            } else if (op_type == E_LIST) {
                return Expr(new ListFunc(parameters));
            } else if (op_type == E_LT) {
                if (parameters.size() == 2) return Expr(new Less(parameters[0], parameters[1]));
                else return Expr(new LessVar(parameters));
            } else if (op_type == E_LE) {
                if (parameters.size() == 2) return Expr(new LessEq(parameters[0], parameters[1]));
                else return Expr(new LessEqVar(parameters));
            } else if (op_type == E_EQ) {
                if (parameters.size() == 2) return Expr(new Equal(parameters[0], parameters[1]));
                else return Expr(new EqualVar(parameters));
            } else if (op_type == E_GE) {
                if (parameters.size() == 2) return Expr(new GreaterEq(parameters[0], parameters[1]));
                else return Expr(new GreaterEqVar(parameters));
            } else if (op_type == E_GT) {
                if (parameters.size() == 2) return Expr(new Greater(parameters[0], parameters[1]));
                else return Expr(new GreaterVar(parameters));
            } else if (op_type == E_AND) {
                return Expr(new AndVar(parameters));
            } else if (op_type == E_OR) {
                return Expr(new OrVar(parameters));
            } else if (op_type == E_NOT) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for not");
                return Expr(new Not(parameters[0]));
            } else if (op_type == E_CONS) {
                if (parameters.size() != 2) throw RuntimeError("Wrong number of arguments for cons");
                return Expr(new Cons(parameters[0], parameters[1]));
            } else if (op_type == E_CAR) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for car");
                return Expr(new Car(parameters[0]));
            } else if (op_type == E_CDR) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for cdr");
                return Expr(new Cdr(parameters[0]));
            } else if (op_type == E_SETCAR) {
                if (parameters.size() != 2) throw RuntimeError("Wrong number of arguments for set-car!");
                return Expr(new SetCar(parameters[0], parameters[1]));
            } else if (op_type == E_SETCDR) {
                if (parameters.size() != 2) throw RuntimeError("Wrong number of arguments for set-cdr!");
                return Expr(new SetCdr(parameters[0], parameters[1]));
            } else if (op_type == E_EQQ) {
                if (parameters.size() != 2) throw RuntimeError("Wrong number of arguments for eq?");
                return Expr(new IsEq(parameters[0], parameters[1]));
            } else if (op_type == E_BOOLQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for boolean?");
                return Expr(new IsBoolean(parameters[0]));
            } else if (op_type == E_INTQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for number?");
                return Expr(new IsFixnum(parameters[0]));
            } else if (op_type == E_NULLQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for null?");
                return Expr(new IsNull(parameters[0]));
            } else if (op_type == E_PAIRQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for pair?");
                return Expr(new IsPair(parameters[0]));
            } else if (op_type == E_PROCQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for procedure?");
                return Expr(new IsProcedure(parameters[0]));
            } else if (op_type == E_SYMBOLQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for symbol?");
                return Expr(new IsSymbol(parameters[0]));
            } else if (op_type == E_LISTQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for list?");
                return Expr(new IsList(parameters[0]));
            } else if (op_type == E_STRINGQ) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for string?");
                return Expr(new IsString(parameters[0]));
            } else if (op_type == E_DISPLAY) {
                if (parameters.size() != 1) throw RuntimeError("Wrong number of arguments for display");
                return Expr(new Display(parameters[0]));
            } else if (op_type == E_VOID) {
                if (parameters.size() != 0) throw RuntimeError("Wrong number of arguments for void");
                return Expr(new MakeVoid());
            } else if (op_type == E_EXIT) {
                if (parameters.size() != 0) throw RuntimeError("Wrong number of arguments for exit");
                return Expr(new Exit());
            }
        }

        if (reserved_words.count(op) != 0) {
            switch (reserved_words[op]) {
                case E_BEGIN: {
                    vector<Expr> exps;
                    for (size_t i = 1; i < stxs.size(); ++i) exps.push_back(stxs[i]->parse(env));
                    return Expr(new Begin(exps));
                }
                case E_QUOTE: {
                    if (stxs.size() != 2) throw RuntimeError("Wrong number of arguments for quote");
                    return Expr(new Quote(stxs[1]));
                }
                case E_IF: {
                    if (stxs.size() == 3) {
                        return Expr(new If(stxs[1]->parse(env), stxs[2]->parse(env), Expr(new MakeVoid())));
                    } else if (stxs.size() == 4) {
                        return Expr(new If(stxs[1]->parse(env), stxs[2]->parse(env), stxs[3]->parse(env)));
                    } else {
                        throw RuntimeError("Wrong number of arguments for if");
                    }
                }
                case E_COND: {
                    std::vector<std::vector<Expr>> clauses;
                    for (size_t i = 1; i < stxs.size(); ++i) {
                        List* clause_list = dynamic_cast<List*>(stxs[i].get());
                        if (clause_list == nullptr) throw RuntimeError("Cond clauses must be lists");
                        std::vector<Expr> clause;
                        for (size_t j = 0; j < clause_list->stxs.size(); ++j) {
                            if (j == 0 && clause_list->stxs[j].get() != nullptr) {
                                SymbolSyntax* sym = dynamic_cast<SymbolSyntax*>(clause_list->stxs[j].get());
                                if (sym != nullptr && sym->s == "else") {
                                    clause.push_back(Expr(new True()));
                                    continue;
                                }
                            }
                            clause.push_back(clause_list->stxs[j]->parse(env));
                        }
                        clauses.push_back(clause);
                    }
                    return Expr(new Cond(clauses));
                }
                case E_LAMBDA: {
                    if (stxs.size() < 3) throw RuntimeError("Wrong number of arguments for lambda");
                    List* args_list = dynamic_cast<List*>(stxs[1].get());
                    if (args_list == nullptr) throw RuntimeError("Lambda arguments must be a list");
                    std::vector<std::string> args;
                    Assoc new_env = env;
                    for (size_t i = 0; i < args_list->stxs.size(); ++i) {
                        SymbolSyntax* sym = dynamic_cast<SymbolSyntax*>(args_list->stxs[i].get());
                        if (sym == nullptr) throw RuntimeError("Lambda arguments must be symbols");
                        args.push_back(sym->s);
                        new_env = extend(sym->s, VoidV(), new_env);
                    }
                    if (stxs.size() == 3) {
                        return Expr(new Lambda(args, stxs[2]->parse(new_env)));
                    } else {
                        std::vector<Expr> body_exprs;
                        for (size_t i = 2; i < stxs.size(); ++i) {
                            body_exprs.push_back(stxs[i]->parse(new_env));
                        }
                        return Expr(new Lambda(args, Expr(new Begin(body_exprs))));
                    }
                }
                case E_DEFINE: {
                    if (stxs.size() < 3) throw RuntimeError("Wrong number of arguments for define");
                    
                    if (SymbolSyntax* sym = dynamic_cast<SymbolSyntax*>(stxs[1].get())) {
                        if (stxs.size() > 3) {
                            std::vector<Expr> body_exprs;
                            for (size_t i = 2; i < stxs.size(); ++i) {
                                body_exprs.push_back(stxs[i]->parse(env));
                            }
                            return Expr(new Define(sym->s, Expr(new Begin(body_exprs))));
                        } else {
                            return Expr(new Define(sym->s, stxs[2]->parse(env)));
                        }
                    } else if (List* func_def = dynamic_cast<List*>(stxs[1].get())) {
                        if (func_def->stxs.empty()) throw RuntimeError("Define function name missing");
                        SymbolSyntax* func_name = dynamic_cast<SymbolSyntax*>(func_def->stxs[0].get());
                        if (func_name == nullptr) throw RuntimeError("Define function name must be symbol");
                        
                        std::vector<std::string> args;
                        Assoc new_env = env;
                        for (size_t i = 1; i < func_def->stxs.size(); ++i) {
                            SymbolSyntax* arg_sym = dynamic_cast<SymbolSyntax*>(func_def->stxs[i].get());
                            if (arg_sym == nullptr) throw RuntimeError("Define function arguments must be symbols");
                            args.push_back(arg_sym->s);
                            new_env = extend(arg_sym->s, VoidV(), new_env);
                        }
                        
                        if (stxs.size() == 3) {
                            return Expr(new Define(func_name->s, Expr(new Lambda(args, stxs[2]->parse(new_env)))));
                        } else {
                            std::vector<Expr> body_exprs;
                            for (size_t i = 2; i < stxs.size(); ++i) {
                                body_exprs.push_back(stxs[i]->parse(new_env));
                            }
                            return Expr(new Define(func_name->s, Expr(new Lambda(args, Expr(new Begin(body_exprs))))));
                        }
                    } else {
                        throw RuntimeError("Invalid define syntax");
                    }
                }
                case E_LET: 
                case E_LETREC: {
                    if (stxs.size() < 3) throw RuntimeError("Wrong number of arguments for let/letrec");
                    List* bind_list = dynamic_cast<List*>(stxs[1].get());
                    if (bind_list == nullptr) throw RuntimeError("Let bindings must be a list");
                    
                    std::vector<std::pair<std::string, Expr>> binds;
                    Assoc new_env = env;
                    // For letrec, binding expressions are parsed in extended env.
                    // For let, they are parsed in original env. But for simplicity, we parse in env, and then track.
                    for (size_t i = 0; i < bind_list->stxs.size(); ++i) {
                        List* bind = dynamic_cast<List*>(bind_list->stxs[i].get());
                        if (bind == nullptr || bind->stxs.size() != 2) throw RuntimeError("Invalid let binding");
                        SymbolSyntax* sym = dynamic_cast<SymbolSyntax*>(bind->stxs[0].get());
                        if (sym == nullptr) throw RuntimeError("Let binding variable must be a symbol");
                        new_env = extend(sym->s, VoidV(), new_env);
                    }
                    
                    for (size_t i = 0; i < bind_list->stxs.size(); ++i) {
                        List* bind = dynamic_cast<List*>(bind_list->stxs[i].get());
                        SymbolSyntax* sym = dynamic_cast<SymbolSyntax*>(bind->stxs[0].get());
                        if (reserved_words[op] == E_LETREC) {
                            binds.push_back({sym->s, bind->stxs[1]->parse(new_env)});
                        } else {
                            binds.push_back({sym->s, bind->stxs[1]->parse(env)});
                        }
                    }
                    
                    Expr body = Expr(nullptr);
                    if (stxs.size() == 3) {
                        body = stxs[2]->parse(new_env);
                    } else {
                        std::vector<Expr> body_exprs;
                        for (size_t i = 2; i < stxs.size(); ++i) {
                            body_exprs.push_back(stxs[i]->parse(new_env));
                        }
                        body = Expr(new Begin(body_exprs));
                    }
                    
                    if (reserved_words[op] == E_LET) {
                        return Expr(new Let(binds, body));
                    } else {
                        return Expr(new Letrec(binds, body));
                    }
                }
                case E_SET: {
                    if (stxs.size() != 3) throw RuntimeError("Wrong number of arguments for set!");
                    SymbolSyntax* sym = dynamic_cast<SymbolSyntax*>(stxs[1].get());
                    if (sym == nullptr) throw RuntimeError("set! target must be a symbol");
                    return Expr(new Set(sym->s, stxs[2]->parse(env)));
                }
                default:
                    throw RuntimeError("Unknown reserved word: " + op);
            }
        }

        // default: use Apply to be an expression
        vector<Expr> parameters;
        for (size_t i = 1; i < stxs.size(); ++i) {
            parameters.push_back(stxs[i]->parse(env));
        }
        return Expr(new Apply(stxs[0]->parse(env), parameters));
    }
}
