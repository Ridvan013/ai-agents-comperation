#include <iostream>
#include <string>

#include "store.hpp"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Store store;
    std::string line;
    while (std::getline(std::cin, line)) {
        // getline strips the trailing '\n'; strip a trailing '\r' too so that
        // CRLF-terminated input is handled the same as LF-terminated input.
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!store.executeLine(line)) break;
    }
    return 0;
}
