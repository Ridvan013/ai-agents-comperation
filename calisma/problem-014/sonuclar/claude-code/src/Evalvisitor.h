#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"
#include "PyValue.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Tree-walking interpreter for the simplified Python grammar.
//
// Only visitFile_input is wired into ANTLR's std::any-based visitor dispatch
// (as the entry point). Everything else is evaluated through statically typed
// helpers that return Value / void directly, which avoids boxing every
// expression result into a std::any (a large, heap-allocating operation) and
// keeps tight loops fast.
class EvalVisitor : public Python3ParserBaseVisitor {
public:
    EvalVisitor() { scopes_.emplace_back(); /* global scope */ }

    std::any visitFile_input(Python3Parser::File_inputContext *ctx) override {
        for (auto *stmt : ctx->stmt()) {
            execStmt(stmt);
            if (flow_ != Flow::NORMAL) break;
        }
        return {};
    }

private:
    // Non-local control flow within a function body / loop.
    enum class Flow { NORMAL, BREAK, CONTINUE, RETURN };

    struct Func {
        std::vector<std::string> params;
        std::vector<Value> defaults;  // aligned with params
        Python3Parser::SuiteContext *body = nullptr;
    };

    using Scope = std::unordered_map<std::string, Value>;

    std::vector<Scope> scopes_;                      // scopes_[0] is global
    std::unordered_map<std::string, Func> functions_;
    Flow flow_ = Flow::NORMAL;
    Value returnValue_;

    // Caches keyed by parse-tree node, populated on first use: constant literals
    // (parsed once instead of on every evaluation) and extracted assignment
    // target names (avoids rebuilding strings via getText() in loops).
    std::unordered_map<Python3Parser::AtomContext *, Value> litCache_;
    std::unordered_map<Python3Parser::TestlistContext *, std::vector<std::string>> targetCache_;

    const std::vector<std::string> &targetNames(Python3Parser::TestlistContext *tl) {
        auto it = targetCache_.find(tl);
        if (it != targetCache_.end()) return it->second;
        std::vector<std::string> names;
        for (auto *t : tl->test()) names.push_back(t->getText());
        return targetCache_.emplace(tl, std::move(names)).first->second;
    }

    // ---- variable resolution ----------------------------------------------
    // Read: current (local) scope first, then global.
    const Value &readVar(const std::string &name) {
        Scope &local = scopes_.back();
        auto it = local.find(name);
        if (it != local.end()) return it->second;
        Scope &global = scopes_.front();
        auto it2 = global.find(name);
        if (it2 != global.end()) return it2->second;
        static const Value none = makeNone();
        return none;
    }

    // Write: update the name where it already lives (local, else global);
    // otherwise create it in the current scope. Assigning to a name that
    // exists globally therefore updates the global (no shadowing) unless the
    // name was introduced locally via a parameter.
    void assignVar(const std::string &name, Value val) {
        Scope &local = scopes_.back();
        auto it = local.find(name);
        if (it != local.end()) {
            it->second = std::move(val);
            return;
        }
        Scope &global = scopes_.front();
        auto it2 = global.find(name);
        if (it2 != global.end()) {
            it2->second = std::move(val);
            return;
        }
        local.emplace(name, std::move(val));
    }

    // ---- statements -------------------------------------------------------
    void execStmt(Python3Parser::StmtContext *ctx) {
        if (auto *s = ctx->simple_stmt()) {
            execSmallStmt(s->small_stmt());
        } else {
            execCompound(ctx->compound_stmt());
        }
    }

    void execCompound(Python3Parser::Compound_stmtContext *ctx) {
        if (auto *s = ctx->if_stmt()) {
            execIf(s);
        } else if (auto *w = ctx->while_stmt()) {
            execWhile(w);
        } else {
            execFuncdef(ctx->funcdef());
        }
    }

    void execSmallStmt(Python3Parser::Small_stmtContext *ctx) {
        if (auto *e = ctx->expr_stmt()) {
            execExprStmt(e);
        } else {
            execFlow(ctx->flow_stmt());
        }
    }

    void execFlow(Python3Parser::Flow_stmtContext *ctx) {
        if (ctx->break_stmt()) {
            flow_ = Flow::BREAK;
        } else if (ctx->continue_stmt()) {
            flow_ = Flow::CONTINUE;
        } else {
            auto *r = ctx->return_stmt();
            returnValue_ = r->testlist() ? evalTestlist(r->testlist()) : makeNone();
            flow_ = Flow::RETURN;
        }
    }

    void execFuncdef(Python3Parser::FuncdefContext *ctx) {
        Func f;
        f.body = ctx->suite();
        if (auto *params = ctx->parameters(); params->typedargslist()) {
            auto *tl = params->typedargslist();
            auto tfps = tl->tfpdef();
            auto tests = tl->test();
            size_t nparams = tfps.size();
            size_t ndef = tests.size();
            for (auto *tf : tfps) f.params.push_back(tf->NAME()->getText());
            f.defaults.assign(nparams, Value());
            // Defaults are guaranteed contiguous at the end of the list.
            for (size_t j = 0; j < ndef; j++) {
                f.defaults[nparams - ndef + j] = evalTest(tests[j]);
            }
        }
        functions_[ctx->NAME()->getText()] = std::move(f);
    }

    void execExprStmt(Python3Parser::Expr_stmtContext *ctx) {
        auto testlists = ctx->testlist();
        if (ctx->augassign()) {
            const std::string &target = targetNames(testlists[0])[0];
            BinOp op = augAssignCode(ctx->augassign());
            Value rhs = evalTestlist(testlists[1]);
            const Value &cur = readVar(target);
            assignVar(target, binaryOp(cur, op, rhs));
        } else if (testlists.size() > 1) {
            Value val = evalTestlist(testlists.back());
            for (size_t i = 0; i + 1 < testlists.size(); i++) {
                assignTarget(testlists[i], val);
            }
        } else {
            evalTestlist(testlists[0]);  // bare expression, evaluated for side effects
        }
    }

    void assignTarget(Python3Parser::TestlistContext *target, const Value &val) {
        const std::vector<std::string> &names = targetNames(target);
        if (target->COMMA().empty()) {  // single (scalar) target
            assignVar(names[0], val);
        } else {  // tuple target: unpack
            const auto &elems = *val.tup;
            for (size_t i = 0; i < names.size(); i++) assignVar(names[i], elems[i]);
        }
    }

    void execIf(Python3Parser::If_stmtContext *ctx) {
        auto tests = ctx->test();
        auto suites = ctx->suite();
        for (size_t i = 0; i < tests.size(); i++) {
            if (toBool(evalTest(tests[i]))) {
                execSuite(suites[i]);
                return;
            }
        }
        if (suites.size() > tests.size()) {
            execSuite(suites.back());  // else branch
        }
    }

    void execWhile(Python3Parser::While_stmtContext *ctx) {
        auto *test = ctx->test();
        auto *suite = ctx->suite();
        while (toBool(evalTest(test))) {
            execSuite(suite);
            if (flow_ == Flow::BREAK) {
                flow_ = Flow::NORMAL;
                break;
            }
            if (flow_ == Flow::CONTINUE) {
                flow_ = Flow::NORMAL;
                continue;
            }
            if (flow_ == Flow::RETURN) break;  // propagate to caller
        }
    }

    void execSuite(Python3Parser::SuiteContext *ctx) {
        if (auto *s = ctx->simple_stmt()) {
            execSmallStmt(s->small_stmt());
        } else {
            for (auto *stmt : ctx->stmt()) {
                execStmt(stmt);
                if (flow_ != Flow::NORMAL) break;
            }
        }
    }

    // ---- expressions ------------------------------------------------------
    // These walk ctx->children directly (casting statically, since the grammar
    // fixes each child's type) rather than calling the generated vector-valued
    // accessors, which rebuild and heap-allocate a vector on every call.
    using PT = antlr4::tree::ParseTree;

    Value evalTest(Python3Parser::TestContext *ctx) {
        return evalOrTest(static_cast<Python3Parser::Or_testContext *>(ctx->children[0]));
    }

    Value evalOrTest(Python3Parser::Or_testContext *ctx) {
        auto &ch = ctx->children;
        Value v = evalAndTest(static_cast<Python3Parser::And_testContext *>(ch[0]));
        for (size_t i = 2; i < ch.size(); i += 2) {  // skip 'or' terminals
            if (toBool(v)) return v;                  // short-circuit, returns the operand
            v = evalAndTest(static_cast<Python3Parser::And_testContext *>(ch[i]));
        }
        return v;
    }

    Value evalAndTest(Python3Parser::And_testContext *ctx) {
        auto &ch = ctx->children;
        Value v = evalNotTest(static_cast<Python3Parser::Not_testContext *>(ch[0]));
        for (size_t i = 2; i < ch.size(); i += 2) {  // skip 'and' terminals
            if (!toBool(v)) return v;                 // short-circuit, returns the operand
            v = evalNotTest(static_cast<Python3Parser::Not_testContext *>(ch[i]));
        }
        return v;
    }

    Value evalNotTest(Python3Parser::Not_testContext *ctx) {
        auto &ch = ctx->children;
        if (ch.size() > 1)  // 'not' not_test
            return makeBool(!toBool(evalNotTest(static_cast<Python3Parser::Not_testContext *>(ch[1]))));
        return evalComparison(static_cast<Python3Parser::ComparisonContext *>(ch[0]));
    }

    Value evalComparison(Python3Parser::ComparisonContext *ctx) {
        auto &ch = ctx->children;
        Value prev = evalArith(static_cast<Python3Parser::Arith_exprContext *>(ch[0]));
        if (ch.size() == 1) return prev;
        bool result = true;
        for (size_t i = 1; i < ch.size(); i += 2) {
            CmpOp op = compOpCode(static_cast<Python3Parser::Comp_opContext *>(ch[i]));
            Value cur = evalArith(static_cast<Python3Parser::Arith_exprContext *>(ch[i + 1]));
            if (!compareOp(prev, op, cur)) {
                result = false;
                break;  // short-circuit remaining comparisons
            }
            prev = std::move(cur);
        }
        return makeBool(result);
    }

    Value evalArith(Python3Parser::Arith_exprContext *ctx) {
        auto &ch = ctx->children;
        Value v = evalTerm(static_cast<Python3Parser::TermContext *>(ch[0]));
        for (size_t i = 1; i < ch.size(); i += 2) {
            BinOp op = addSubCode(static_cast<Python3Parser::Addorsub_opContext *>(ch[i]));
            v = binaryOp(v, op, evalTerm(static_cast<Python3Parser::TermContext *>(ch[i + 1])));
        }
        return v;
    }

    Value evalTerm(Python3Parser::TermContext *ctx) {
        auto &ch = ctx->children;
        Value v = evalFactor(static_cast<Python3Parser::FactorContext *>(ch[0]));
        for (size_t i = 1; i < ch.size(); i += 2) {
            BinOp op = mulDivCode(static_cast<Python3Parser::Muldivmod_opContext *>(ch[i]));
            v = binaryOp(v, op, evalFactor(static_cast<Python3Parser::FactorContext *>(ch[i + 1])));
        }
        return v;
    }

    Value evalFactor(Python3Parser::FactorContext *ctx) {
        auto &ch = ctx->children;
        if (ch.size() == 1)  // atom_expr
            return evalAtomExpr(static_cast<Python3Parser::Atom_exprContext *>(ch[0]));
        Value v = evalFactor(static_cast<Python3Parser::FactorContext *>(ch[1]));
        auto *tok = static_cast<antlr4::tree::TerminalNode *>(ch[0]);
        if (tok->getSymbol()->getType() == Python3Parser::MINUS) return negate(v);
        return unaryPlus(v);
    }

    Value evalAtomExpr(Python3Parser::Atom_exprContext *ctx) {
        if (!ctx->trailer()) return evalAtom(ctx->atom());
        return doCall(ctx->atom()->NAME()->getText(), ctx->trailer());
    }

    static CmpOp compOpCode(Python3Parser::Comp_opContext *c) {
        switch (static_cast<antlr4::tree::TerminalNode *>(c->children[0])->getSymbol()->getType()) {
            case Python3Parser::LESS_THAN:
                return CmpOp::LT;
            case Python3Parser::GREATER_THAN:
                return CmpOp::GT;
            case Python3Parser::EQUALS:
                return CmpOp::EQ;
            case Python3Parser::GT_EQ:
                return CmpOp::GE;
            case Python3Parser::LT_EQ:
                return CmpOp::LE;
            default:
                return CmpOp::NE;  // NOT_EQ_2 ('!=')
        }
    }

    static BinOp addSubCode(Python3Parser::Addorsub_opContext *c) {
        return static_cast<antlr4::tree::TerminalNode *>(c->children[0])->getSymbol()->getType() ==
                       Python3Parser::ADD
                   ? BinOp::ADD
                   : BinOp::SUB;
    }

    static BinOp mulDivCode(Python3Parser::Muldivmod_opContext *c) {
        switch (static_cast<antlr4::tree::TerminalNode *>(c->children[0])->getSymbol()->getType()) {
            case Python3Parser::STAR:
                return BinOp::MUL;
            case Python3Parser::DIV:
                return BinOp::DIV;
            case Python3Parser::IDIV:
                return BinOp::FLOORDIV;
            default:
                return BinOp::MOD;
        }
    }

    static BinOp augAssignCode(Python3Parser::AugassignContext *c) {
        switch (static_cast<antlr4::tree::TerminalNode *>(c->children[0])->getSymbol()->getType()) {
            case Python3Parser::ADD_ASSIGN:
                return BinOp::ADD;
            case Python3Parser::SUB_ASSIGN:
                return BinOp::SUB;
            case Python3Parser::MULT_ASSIGN:
                return BinOp::MUL;
            case Python3Parser::DIV_ASSIGN:
                return BinOp::DIV;
            case Python3Parser::IDIV_ASSIGN:
                return BinOp::FLOORDIV;
            default:
                return BinOp::MOD;
        }
    }

    Value evalAtom(Python3Parser::AtomContext *ctx) {
        auto *c0 = ctx->children[0];
        auto *term = dynamic_cast<antlr4::tree::TerminalNode *>(c0);
        if (!term) return evalFormatString(ctx->format_string());  // only non-terminal first child
        switch (term->getSymbol()->getType()) {
            case Python3Parser::NAME:
                return readVar(term->getText());
            case Python3Parser::NONE:
                return makeNone();
            case Python3Parser::TRUE:
                return makeBool(true);
            case Python3Parser::FALSE:
                return makeBool(false);
            case Python3Parser::OPEN_PAREN:
                return evalTest(ctx->test());  // '(' test ')'
            default:
                break;  // NUMBER or STRING: constant, cache below
        }
        auto it = litCache_.find(ctx);
        if (it != litCache_.end()) return it->second;
        Value v;
        if (term->getSymbol()->getType() == Python3Parser::NUMBER) {
            const std::string &text = term->getText();
            v = (text.find('.') != std::string::npos) ? makeFloat(std::stod(text))
                                                       : makeInt(BigInt::fromString(text));
        } else {  // STRING+ : adjacent literals are concatenated
            std::string out;
            for (auto *strTok : ctx->STRING()) {
                const std::string &t = strTok->getText();
                if (t.size() >= 2) out += t.substr(1, t.size() - 2);  // strip quotes
            }
            v = makeStr(std::move(out));
        }
        litCache_.emplace(ctx, v);
        return v;
    }

    Value evalFormatString(Python3Parser::Format_stringContext *ctx) {
        std::string out;
        for (auto *child : ctx->children) {
            if (auto *tl = dynamic_cast<Python3Parser::TestlistContext *>(child)) {
                out += toStr(evalTestlist(tl));
            } else if (auto *tn = dynamic_cast<antlr4::tree::TerminalNode *>(child)) {
                if (tn->getSymbol()->getType() == Python3Parser::FORMAT_STRING_LITERAL) {
                    out += unescapeFormatLiteral(tn->getText());
                }
            }
        }
        return makeStr(std::move(out));
    }

    static std::string unescapeFormatLiteral(const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); i++) {
            if ((s[i] == '{' || s[i] == '}') && i + 1 < s.size() && s[i + 1] == s[i]) {
                out += s[i];
                i++;  // collapse '{{' -> '{' and '}}' -> '}'
            } else {
                out += s[i];
            }
        }
        return out;
    }

    Value evalTestlist(Python3Parser::TestlistContext *ctx) {
        auto tests = ctx->test();
        if (ctx->COMMA().empty()) return evalTest(tests[0]);
        std::vector<Value> elems;
        elems.reserve(tests.size());
        for (auto *t : tests) elems.push_back(evalTest(t));
        return makeTuple(std::move(elems));
    }

    // ---- calls ------------------------------------------------------------
    Value doCall(const std::string &name, Python3Parser::TrailerContext *trailer) {
        std::vector<Value> pos;
        std::vector<std::pair<std::string, Value>> kw;
        if (trailer->arglist()) {
            for (auto *arg : trailer->arglist()->argument()) {
                if (arg->ASSIGN()) {
                    kw.emplace_back(arg->test(0)->getText(), evalTest(arg->test(1)));
                } else {
                    pos.push_back(evalTest(arg->test(0)));
                }
            }
        }

        if (name == "print") return builtinPrint(pos);
        if (name == "int") return convInt(pos[0]);
        if (name == "float") return convFloat(pos[0]);
        if (name == "str") return convStr(pos[0]);
        if (name == "bool") return pos.empty() ? makeBool(false) : convBool(pos[0]);
        return callUser(name, pos, kw);
    }

    Value builtinPrint(const std::vector<Value> &args) {
        std::string out;
        for (size_t i = 0; i < args.size(); i++) {
            if (i) out += ' ';
            out += toStr(args[i]);
        }
        out += '\n';
        std::cout << out;
        return makeNone();
    }

    Value callUser(const std::string &name, std::vector<Value> &pos,
                   std::vector<std::pair<std::string, Value>> &kw) {
        Func &f = functions_.at(name);
        Scope local;
        local.reserve(f.params.size() * 2);
        for (size_t i = 0; i < pos.size(); i++) local[f.params[i]] = std::move(pos[i]);
        for (auto &p : kw) local[p.first] = std::move(p.second);
        for (size_t i = 0; i < f.params.size(); i++) {
            if (local.find(f.params[i]) == local.end()) local[f.params[i]] = f.defaults[i];
        }

        scopes_.push_back(std::move(local));
        flow_ = Flow::NORMAL;
        execSuite(f.body);
        Value result = (flow_ == Flow::RETURN) ? std::move(returnValue_) : makeNone();
        flow_ = Flow::NORMAL;
        scopes_.pop_back();
        return result;
    }
};

#endif  // PYTHON_INTERPRETER_EVALVISITOR_H
