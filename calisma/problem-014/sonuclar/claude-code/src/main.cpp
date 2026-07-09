#include "Evalvisitor.h"
#include "Python3Lexer.h"
#include "Python3Parser.h"
#include "antlr4-runtime.h"
#include <iostream>
#include <pthread.h>

using namespace antlr4;

// TODO: regenerating files in directory named "generated" is dangerous.
//       if you really need to regenerate,please ask TA for help.

namespace {
tree::ParseTree *g_tree = nullptr;
EvalVisitor *g_visitor = nullptr;

void *runInterpreter(void *) {
    g_visitor->visit(g_tree);
    return nullptr;
}
}  // namespace

int main(int argc, const char *argv[]) {
    // TODO: please don't modify the code below the construction of ifs if you want to use visitor mode
    ANTLRInputStream input(std::cin);
    Python3Lexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    tokens.fill();
    Python3Parser parser(&tokens);
    tree::ParseTree *tree = parser.file_input();
    EvalVisitor visitor;

    // Recursion depth may reach ~2000 Python frames, each of which nests many
    // C++ visitor frames. Run the traversal on a thread with a large stack so
    // deep recursion cannot overflow the default stack.
    std::ios::sync_with_stdio(false);
    g_tree = tree;
    g_visitor = &visitor;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, static_cast<size_t>(256) * 1024 * 1024);
    pthread_t thread;
    if (pthread_create(&thread, &attr, runInterpreter, nullptr) == 0) {
        pthread_join(thread, nullptr);
    } else {
        runInterpreter(nullptr); // fallback: run on the main thread
    }
    pthread_attr_destroy(&attr);

    std::cout.flush();
    return 0;
}
