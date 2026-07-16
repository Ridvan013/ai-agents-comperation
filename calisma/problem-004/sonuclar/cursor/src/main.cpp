#include "bookstore.hpp"

#include <iostream>
#include <string>

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(0);

  Bookstore store;
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!store.process_line(line)) break;
  }
  return 0;
}
