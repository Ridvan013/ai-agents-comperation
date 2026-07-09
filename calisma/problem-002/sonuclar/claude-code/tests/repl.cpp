// A tiny REPL over the int2048 interface, driven by tests/reference_test.py.
//
// Each request is three whitespace-separated tokens: an operator followed by
// two operands. The result (one per line) is compared against Python's
// arbitrary-precision integers by the reference harness.
#include "int2048.h"

#include <string>

int main() {
  using sjtu::int2048;
  std::string op;
  while (std::cin >> op) {
    int2048 a, b;
    std::cin >> a >> b;
    if (op == "+")
      std::cout << (a + b);
    else if (op == "-")
      std::cout << (a - b);
    else if (op == "*")
      std::cout << (a * b);
    else if (op == "/")
      std::cout << (a / b);
    else if (op == "%")
      std::cout << (a % b);
    else if (op == "neg")
      std::cout << (-a);
    else if (op == "<")
      std::cout << (a < b ? 1 : 0);
    else if (op == "==")
      std::cout << (a == b ? 1 : 0);
    else if (op == "add") { // in-place member add
      int2048 c = a;
      c.add(b);
      std::cout << c;
    } else if (op == "minus") { // in-place member minus
      int2048 c = a;
      c.minus(b);
      std::cout << c;
    }
    std::cout << '\n';
  }
  return 0;
}
