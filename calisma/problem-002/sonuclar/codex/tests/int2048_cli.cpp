#include "int2048.h"

#include <iostream>
#include <string>

int main() {
  int query_count = 0;
  if (!(std::cin >> query_count)) {
    return 0;
  }

  for (int i = 0; i < query_count; ++i) {
    std::string operation;
    sjtu::int2048 left;
    sjtu::int2048 right;
    std::cin >> operation >> left >> right;

    if (operation == "add") {
      std::cout << (left + right);
    } else if (operation == "sub") {
      std::cout << (left - right);
    } else if (operation == "mul") {
      std::cout << (left * right);
    } else if (operation == "div") {
      std::cout << (left / right);
    } else if (operation == "mod") {
      std::cout << (left % right);
    } else if (operation == "cmp") {
      std::cout << (left < right ? -1 : (left > right ? 1 : 0));
    }

    if (i + 1 != query_count) {
      std::cout << '\n';
    }
  }
  return 0;
}
