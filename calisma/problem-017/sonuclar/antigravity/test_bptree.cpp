#include "bptree.hpp"
#include "models.hpp"
#include <iostream>

using namespace sjtu;

int main() {
    BPTree<StationTrainKey, int, 10> tree("test_tree.dat");
    tree.insert(StationTrainKey("A", "T1"), 1);
    tree.insert(StationTrainKey("A", "T2"), 2);
    tree.insert(StationTrainKey("B", "T1"), 3);
    
    auto match = [](const StationTrainKey& k) {
        return strcmp(k.station, "A") == 0;
    };
    sjtu::vector<int> res = tree.find_prefix(StationTrainKey("A", ""), match);
    for (size_t i = 0; i < res.size(); ++i) {
        std::cout << res[i] << " ";
    }
    std::cout << "\n";
    return 0;
}
