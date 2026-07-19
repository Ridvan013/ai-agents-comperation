#include <iostream>
#include <string>
#include "bpt.hpp"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int n;
    if (!(std::cin >> n)) return 0;

    BPlusTree tree("data.dat");

    for (int i = 0; i < n; ++i) {
        std::string cmd;
        std::cin >> cmd;
        if (cmd == "insert") {
            std::string key;
            int val;
            std::cin >> key >> val;
            tree.insert(key.c_str(), val);
        } else if (cmd == "delete") {
            std::string key;
            int val;
            std::cin >> key >> val;
            tree.remove(key.c_str(), val);
        } else if (cmd == "find") {
            std::string key;
            std::cin >> key;
            tree.find(key.c_str());
        }
    }

    return 0;
}
