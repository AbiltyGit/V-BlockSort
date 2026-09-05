#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <string>
#include <cstring>
#include "../include/vblock/vblock_sort.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <gnu|vblock> <size>\n";
        return 1;
    }

    std::string mode = argv[1];
    size_t size = std::stoull(argv[2]);

    std::mt19937 rng(42);
    std::vector<int32_t> data(size);
    for (size_t i = 0; i < size; ++i) data[i] = rng();

    // Repeat 5 times to get substantial sample for perf
    for (int iter = 0; iter < 5; ++iter) {
        std::vector<int32_t> copy = data;
        if (mode == "gnu") {
            std::__inplace_stable_sort(copy.begin(), copy.end(), std::less<int32_t>());
        } else if (mode == "vblock") {
            VBlock::Sort<4096>(copy.data(), copy.size(), std::less<int32_t>());
        }
    }

    return 0;
}
