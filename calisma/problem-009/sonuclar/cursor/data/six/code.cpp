#include <iostream>
#include <chrono>
#include "vector.hpp"

using std::cout;
using std::endl;
using std::cerr;
using namespace std::chrono;
using namespace sjtu;

// Threshold for detecting expansion (microseconds)
constexpr int EXPANSION_THRESHOLD_US = 100;

int main() {
    vector<int> vec;
    
    // Phase 1: push_back 1e6 elements
    for (int i = 0; i < 1000000; ++i) {
        vec.push_back(i);
    }
    
    // Record current size
    size_t initial_size = vec.size();
    
    // Continue pushing until expansion is detected (determined by time)
    size_t expansion_size = initial_size;
    auto last_time = high_resolution_clock::now();
    bool expansion_detected = false;
    
    while (!expansion_detected && vec.size() < 8000000) {  // Set upper limit to prevent infinite loop
        vec.push_back(vec.size());
        auto current_time = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(current_time - last_time).count();
        
        if (duration > EXPANSION_THRESHOLD_US) {
            expansion_detected = true;
            expansion_size = vec.size();
        }
        last_time = current_time;
    }
    
    cout << "Initial size: " << initial_size << endl;
    cerr << "Size at expansion: " << expansion_size << endl;
    
    // Repeatedly perform push and pop operations near the expansion point
    auto start = high_resolution_clock::now();
    for (int round = 0; round < 100000; ++round) {
        // Pop 16 times per round
        for (int i = 0; i < 16; ++i) {
            vec.pop_back();
        }
        // Push 16 times
        for (int i = 0; i < 16; ++i) {
            vec.push_back(round);
        }
    }
    auto end = high_resolution_clock::now();
    
    cerr << "Time taken for 100000 rounds of 16 push-pops (ms): " 
         << duration_cast<milliseconds>(end - start).count() << endl;
    cerr << "Final size: " << vec.size() << endl;
    
    return 0;
}