#include <iostream>
#include <string>

#include "bookstore.hpp"

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  bookstore::Bookstore store;
  store.open();

  std::string line;
  while (std::getline(std::cin, line)) {
    // Tolerate CRLF input: a trailing '\r' would otherwise end up inside the
    // last token and make every command illegal.
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!store.executeLine(line)) break;
  }

  store.close();  // Flush every cached block before exiting.
  return 0;
}
