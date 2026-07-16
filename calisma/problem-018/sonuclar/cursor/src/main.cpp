#include "Def.hpp"
#include "syntax.hpp"
#include "expr.hpp"
#include "value.hpp"
#include "RE.hpp"
#include <sstream>
#include <iostream>
#include <map>

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

// Defined in syntax.cpp: skips whitespace and comments in the stream.
std::istream &readSpace(std::istream &is);

// A void RESULT is shown as "#<void>" when it is meaningful data (e.g. the
// result of (void), or void retrieved via car/cdr). It is suppressed only for
// side-effecting statement forms whose purpose is the effect, not the value:
// define, set!, display, set-car!, set-cdr! (and begin ending in such a form).
bool suppressVoidOutput(Expr expr) {
    if (dynamic_cast<Define *>(expr.get()) != nullptr)
        return true;
    if (dynamic_cast<Set *>(expr.get()) != nullptr)
        return true;
    if (dynamic_cast<Display *>(expr.get()) != nullptr)
        return true;
    if (dynamic_cast<SetCar *>(expr.get()) != nullptr)
        return true;
    if (dynamic_cast<SetCdr *>(expr.get()) != nullptr)
        return true;

    Begin *begin_expr = dynamic_cast<Begin *>(expr.get());
    if (begin_expr != nullptr && !begin_expr->es.empty())
        return suppressVoidOutput(begin_expr->es.back());
    return false;
}

void REPL() {
    Assoc global_env = empty();
    while (1) {
#ifndef ONLINE_JUDGE
        std::cout << "scm> ";
#endif
        // Stop cleanly at end-of-input (after skipping trailing whitespace and
        // comments) so the REPL does not loop forever when no (exit) is given.
        readSpace(std::cin);
        if (std::cin.peek() == EOF)
            break;
        Syntax stx = readSyntax(std::cin);
        try {
            Expr expr = stx->parse(global_env);
            Value val = expr->eval(global_env);
            if (val->v_type == V_TERMINATE)
                break;
            if (val->v_type == V_VOID) {
                if (!suppressVoidOutput(expr))
                    val->show(std::cout);
            } else {
                val->show(std::cout);
            }
        } catch (const RuntimeError &RE) {
#ifdef DEBUG_RE
            std::cout << "RuntimeError: " << RE.message();
#else
            std::cout << "RuntimeError";
#endif
        }
        puts("");
    }
}

#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
static void *replThread(void *) {
    REPL();
    return nullptr;
}
#endif

int main(int argc, char *argv[]) {
#if defined(__unix__) || defined(__APPLE__)
    // Run on a thread with a large stack so deep (non-tail-optimized)
    // recursion in the interpreted program does not overflow the C++ stack.
    // The reservation is lazy: only pages actually touched count as memory.
    pthread_attr_t attr;
    pthread_t tid;
    const size_t stack_size = (size_t)1024 * 1024 * 1024; // 1 GiB reservation
    if (pthread_attr_init(&attr) == 0 &&
        pthread_attr_setstacksize(&attr, stack_size) == 0 &&
        pthread_create(&tid, &attr, replThread, nullptr) == 0) {
        pthread_join(tid, nullptr);
        pthread_attr_destroy(&attr);
        return 0;
    }
#endif
    REPL();
    return 0;
}
