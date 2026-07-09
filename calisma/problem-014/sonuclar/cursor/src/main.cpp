#include "Evalvisitor.h"
#include "Python3Lexer.h"
#include "Python3Parser.h"
#include "antlr4-runtime.h"
#include <iostream>
#include <pthread.h>
using namespace antlr4;
// TODO: regenerating files in directory named "generated" is dangerous.
//       if you really need to regenerate,please ask TA for help.

static tree::ParseTree *g_tree = nullptr;

// Executed on a dedicated large-stack thread (see main) so that deep Python
// recursion (guaranteed up to 2000 levels) cannot overflow the default stack.
static void *runInterpreter(void *) {
	EvalVisitor visitor;
	visitor.visit(g_tree);
	return nullptr;
}

int main(int argc, const char *argv[]) {
	std::ios::sync_with_stdio(false);
	// TODO: please don't modify the code below the construction of ifs if you want to use visitor mode
	ANTLRInputStream input(std::cin);
	Python3Lexer lexer(&input);
	CommonTokenStream tokens(&lexer);
	tokens.fill();
	Python3Parser parser(&tokens);
	tree::ParseTree *tree = parser.file_input();
	g_tree = tree;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, static_cast<size_t>(1) << 30); // 1 GiB
	pthread_t worker;
	if (pthread_create(&worker, &attr, runInterpreter, nullptr) == 0) {
		pthread_join(worker, nullptr);
	} else {
		runInterpreter(nullptr); // fallback to the main thread
	}
	pthread_attr_destroy(&attr);

	std::cout.flush();
	return 0;
}
