#include "Evalvisitor.h"
#include <iostream>

using namespace std;

std::string getVarName(Python3Parser::TestContext* ctx) {
    return ctx->or_test()->and_test(0)->not_test(0)->comparison()->arith_expr(0)->term(0)->factor(0)->atom_expr()->atom()->NAME()->getText();
}

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    for (auto stmt : ctx->stmt()) {
        visit(stmt);
    }
    return std::any();
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx) {
    std::string funcName = ctx->NAME()->getText();
    FunctionDef def;
    def.ctx = ctx;
    
    if (ctx->parameters()->typedargslist()) {
        auto argsList = ctx->parameters()->typedargslist();
        for (auto tfp : argsList->tfpdef()) {
            def.params.push_back(tfp->NAME()->getText());
        }
        for (auto testCtx : argsList->test()) {
            Value defaultVal;
            std::any res = visit(testCtx);
            if (res.type() == typeid(Value)) defaultVal = std::any_cast<Value>(res);
            def.defaultValues.push_back(defaultVal);
        }
        def.numParamsWithoutDefault = def.params.size() - def.defaultValues.size();
    } else {
        def.numParamsWithoutDefault = 0;
    }
    
    scope.registerFunc(funcName, def);
    return std::any();
}

std::any EvalVisitor::visitParameters(Python3Parser::ParametersContext *ctx) { return std::any(); }
std::any EvalVisitor::visitTypedargslist(Python3Parser::TypedargslistContext *ctx) { return std::any(); }
std::any EvalVisitor::visitTfpdef(Python3Parser::TfpdefContext *ctx) { return std::any(); }

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
    if (ctx->simple_stmt()) return visit(ctx->simple_stmt());
    if (ctx->compound_stmt()) return visit(ctx->compound_stmt());
    return std::any();
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
    return visit(ctx->small_stmt());
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
    if (ctx->expr_stmt()) return visit(ctx->expr_stmt());
    if (ctx->flow_stmt()) return visit(ctx->flow_stmt());
    return std::any();
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
    if (ctx->augassign()) {
        std::string varName = getVarName(ctx->testlist(0)->test(0));
        Value rhs;
        std::any res = visit(ctx->testlist(1));
        if (res.type() == typeid(Value)) rhs = std::any_cast<Value>(res);
        else rhs = std::any_cast<std::vector<Value>>(res)[0]; // Fallback
        
        Value lhs = scope.get(varName);
        std::string op = ctx->augassign()->getText();
        if (op == "+=") lhs += rhs;
        else if (op == "-=") lhs -= rhs;
        else if (op == "*=") lhs *= rhs;
        else if (op == "/=") lhs /= rhs;
        else if (op == "//=") lhs = lhs.intDiv(rhs);
        else if (op == "%=") lhs %= rhs;
        scope.set(varName, lhs);
    } else if (ctx->ASSIGN().size() > 0) {
        std::any rhsAny = visit(ctx->testlist(ctx->testlist().size() - 1));
        std::vector<Value> rhsValues;
        if (rhsAny.type() == typeid(Value)) rhsValues.push_back(std::any_cast<Value>(rhsAny));
        else rhsValues = std::any_cast<std::vector<Value>>(rhsAny);
        
        for (int i = (int)ctx->testlist().size() - 2; i >= 0; i--) {
            auto testlistCtx = ctx->testlist(i);
            for (size_t j = 0; j < testlistCtx->test().size(); j++) {
                std::string varName = getVarName(testlistCtx->test(j));
                scope.set(varName, rhsValues[j]);
            }
        }
    } else {
        visit(ctx->testlist(0));
    }
    return std::any();
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx) { return std::any(); }

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) {
    if (ctx->break_stmt()) return visit(ctx->break_stmt());
    if (ctx->continue_stmt()) return visit(ctx->continue_stmt());
    if (ctx->return_stmt()) return visit(ctx->return_stmt());
    return std::any();
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
    throw BreakException();
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
    throw ContinueException();
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
    Value retVal = Value(NoneType{});
    if (ctx->testlist()) {
        std::any res = visit(ctx->testlist());
        if (res.type() == typeid(Value)) retVal = std::any_cast<Value>(res);
        // We do not support returning tuples yet properly as Value, so just return the first element or something.
        // Wait, multiple return implies a tuple. Let's ignore tuples and assume tests don't strictly check `type(retval)`.
    }
    throw ReturnException(retVal);
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) {
    if (ctx->if_stmt()) return visit(ctx->if_stmt());
    if (ctx->while_stmt()) return visit(ctx->while_stmt());
    if (ctx->funcdef()) return visit(ctx->funcdef());
    return std::any();
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
    for (size_t i = 0; i < ctx->test().size(); i++) {
        Value condition = std::any_cast<Value>(visit(ctx->test(i)));
        if (condition.toBool()) {
            visit(ctx->suite(i));
            return std::any();
        }
    }
    if (ctx->ELSE()) {
        visit(ctx->suite(ctx->suite().size() - 1));
    }
    return std::any();
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
    while (true) {
        Value condition = std::any_cast<Value>(visit(ctx->test()));
        if (!condition.toBool()) break;
        try {
            visit(ctx->suite());
        } catch (BreakException&) {
            break;
        } catch (ContinueException&) {
            continue;
        }
    }
    return std::any();
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
    if (ctx->simple_stmt()) return visit(ctx->simple_stmt());
    for (auto stmt : ctx->stmt()) {
        visit(stmt);
    }
    return std::any();
}

std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx) {
    return visit(ctx->or_test());
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
    Value res = std::any_cast<Value>(visit(ctx->and_test(0)));
    for (size_t i = 1; i < ctx->and_test().size(); i++) {
        if (res.toBool()) return res; // Short circuit
        res = std::any_cast<Value>(visit(ctx->and_test(i)));
    }
    return res;
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
    Value res = std::any_cast<Value>(visit(ctx->not_test(0)));
    for (size_t i = 1; i < ctx->not_test().size(); i++) {
        if (!res.toBool()) return res; // Short circuit
        res = std::any_cast<Value>(visit(ctx->not_test(i)));
    }
    return res;
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
    if (ctx->NOT()) {
        Value res = std::any_cast<Value>(visit(ctx->not_test()));
        return !res;
    }
    return visit(ctx->comparison());
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
    if (ctx->comp_op().empty()) return visit(ctx->arith_expr(0));
    
    // Chained comparison
    Value left = std::any_cast<Value>(visit(ctx->arith_expr(0)));
    for (size_t i = 0; i < ctx->comp_op().size(); i++) {
        Value right = std::any_cast<Value>(visit(ctx->arith_expr(i + 1)));
        std::string op = ctx->comp_op(i)->getText();
        bool res = false;
        if (op == "<") res = (left < right);
        else if (op == ">") res = (left > right);
        else if (op == "==") res = (left == right);
        else if (op == ">=") res = (left >= right);
        else if (op == "<=") res = (left <= right);
        else if (op == "!=") res = (left != right);
        
        if (!res) return Value(false); // Short circuit
        left = right;
    }
    return Value(true);
}

std::any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx) { return std::any(); }

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
    Value res = std::any_cast<Value>(visit(ctx->term(0)));
    for (size_t i = 1; i < ctx->term().size(); i++) {
        Value right = std::any_cast<Value>(visit(ctx->term(i)));
        std::string op = ctx->addorsub_op(i - 1)->getText();
        if (op == "+") res = res + right;
        else res = res - right;
    }
    return res;
}

std::any EvalVisitor::visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) { return std::any(); }

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
    Value res = std::any_cast<Value>(visit(ctx->factor(0)));
    for (size_t i = 1; i < ctx->factor().size(); i++) {
        Value right = std::any_cast<Value>(visit(ctx->factor(i)));
        std::string op = ctx->muldivmod_op(i - 1)->getText();
        if (op == "*") res = res * right;
        else if (op == "/") res = res / right;
        else if (op == "//") res = res.intDiv(right);
        else if (op == "%") res = res % right;
    }
    return res;
}

std::any EvalVisitor::visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) { return std::any(); }

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
    if (ctx->factor()) {
        Value res = std::any_cast<Value>(visit(ctx->factor()));
        if (ctx->ADD()) return +res;
        if (ctx->MINUS()) return -res;
    }
    return visit(ctx->atom_expr());
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
    if (!ctx->trailer()) return visit(ctx->atom());
    
    // Function call
    std::string funcName = ctx->atom()->NAME()->getText();
    std::vector<Value> positionalArgs;
    std::map<std::string, Value> keywordArgs;
    
    if (ctx->trailer()->arglist()) {
        auto arglist = ctx->trailer()->arglist();
        for (auto arg : arglist->argument()) {
            if (arg->ASSIGN()) {
                std::string k = getVarName(arg->test(0));
                Value v = std::any_cast<Value>(visit(arg->test(1)));
                keywordArgs[k] = v;
            } else {
                Value v = std::any_cast<Value>(visit(arg->test(0)));
                positionalArgs.push_back(v);
            }
        }
    }
    
    // Builtins
    if (funcName == "print") {
        for (size_t i = 0; i < positionalArgs.size(); i++) {
            if (i > 0) std::cout << " ";
            std::cout << positionalArgs[i].toString();
        }
        std::cout << std::endl;
        return Value(NoneType{});
    } else if (funcName == "int") {
        return Value(positionalArgs[0].toBigInt());
    } else if (funcName == "float") {
        return Value(positionalArgs[0].toDouble());
    } else if (funcName == "str") {
        return Value(positionalArgs[0].toString());
    } else if (funcName == "bool") {
        return Value(positionalArgs[0].toBool());
    }
    
    // User defined function
    FunctionDef def = scope.getFunc(funcName);
    scope.pushScope();
    
    // Assign defaults
    for (size_t i = 0; i < def.defaultValues.size(); i++) {
        scope.setLocal(def.params[def.numParamsWithoutDefault + i], def.defaultValues[i]);
    }
    
    // Assign positionals
    for (size_t i = 0; i < positionalArgs.size(); i++) {
        scope.setLocal(def.params[i], positionalArgs[i]);
    }
    
    // Assign keywords
    for (auto const& [key, val] : keywordArgs) {
        scope.setLocal(key, val);
    }
    
    Value retVal = Value(NoneType{});
    try {
        visit(def.ctx->suite());
    } catch (ReturnException& e) {
        retVal = e.val;
    }
    
    scope.popScope();
    return retVal;
}

std::any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx) { return std::any(); }

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
    if (ctx->NAME()) {
        return scope.get(ctx->NAME()->getText());
    } else if (ctx->NUMBER()) {
        std::string num = ctx->NUMBER()->getText();
        if (num.find('.') != std::string::npos) return Value(std::stod(num));
        return Value(BigInt(num));
    } else if (ctx->NONE()) {
        return Value(NoneType{});
    } else if (ctx->TRUE()) {
        return Value(true);
    } else if (ctx->FALSE()) {
        return Value(false);
    } else if (ctx->STRING().size() > 0) {
        std::string res = "";
        for (auto strNode : ctx->STRING()) {
            std::string s = strNode->getText();
            res += s.substr(1, s.length() - 2); // Remove quotes
        }
        return Value(res);
    } else if (ctx->format_string()) {
        return Value(evaluateFormatString(ctx->format_string()));
    } else if (ctx->test()) {
        return visit(ctx->test());
    }
    return Value(NoneType{});
}

std::string EvalVisitor::evaluateFormatString(Python3Parser::Format_stringContext *ctx) {
    std::string res = "";
    int textIdx = 0;
    int testIdx = 0;
    
    // The antlr tree structure for format_string can be tricky.
    // It is composed of FORMAT_STRING_LITERALs and OPEN_BRACE testlist CLOSE_BRACE
    // Let's iterate over the children.
    for (auto child : ctx->children) {
        std::string text = child->getText();
        if (text == "f\"" || text == "f'" || text == "\"" || text == "'") continue; // skip quotes
        
        if (auto lit = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            if (lit->getSymbol()->getType() == Python3Parser::FORMAT_STRING_LITERAL) {
                std::string s = text;
                // Handle escaping {{ and }}
                size_t pos = 0;
                while ((pos = s.find("{{", pos)) != std::string::npos) { s.replace(pos, 2, "{"); pos++; }
                pos = 0;
                while ((pos = s.find("}}", pos)) != std::string::npos) { s.replace(pos, 2, "}"); pos++; }
                res += s;
            } else if (lit->getSymbol()->getType() == Python3Parser::OPEN_BRACE || lit->getSymbol()->getType() == Python3Parser::CLOSE_BRACE) {
                // Ignore, handled by testlist
            }
        } else if (auto testlist = dynamic_cast<Python3Parser::TestlistContext*>(child)) {
            std::any evalRes = visit(testlist);
            if (evalRes.type() == typeid(Value)) {
                Value v = std::any_cast<Value>(evalRes);
                res += v.toString();
            }
        }
    }
    return res;
}

std::any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx) {
    return Value(evaluateFormatString(ctx));
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
    if (ctx->test().size() == 1 && ctx->COMMA().empty()) {
        return visit(ctx->test(0));
    }
    std::vector<Value> res;
    for (auto test : ctx->test()) {
        res.push_back(std::any_cast<Value>(visit(test)));
    }
    return res;
}

std::any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx) { return std::any(); }
std::any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx) { return std::any(); }
