#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include "vblock/vblock_sort.hpp"

struct Item {
    int64_t key;
    uint32_t id;

    bool operator<(const Item& o) const { return key < o.key; }
};

bool verify(const std::vector<Item>& v) {
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i].key < v[i - 1].key) return false;
        if (v[i].key == v[i - 1].key && v[i].id < v[i - 1].id) return false;
    }
    return true;
}

int main() {
    std::cout << "Running V-BlockSort standalone unit tests...\n";

    // 1. Edge sizes: 0, 1, 2, 3, ..., 64
    for (size_t n = 0; n <= 64; ++n) {
        std::vector<Item> v(n);
        for (size_t i = 0; i < n; ++i) v[i] = {static_cast<int64_t>(n - i), static_cast<uint32_t>(i)};
        VBlock::Sort(v.begin(), v.end());
        assert(verify(v));
    }
    std::cout << "  [PASS] Small edge sizes (N = 0 to 64)\n";

    // 2. Fuzzing with random arrays & varied cardinalities
    std::mt19937 rng(42);
    for (size_t trial = 0; trial < 200; ++trial) {
        size_t n = rng() % 2000 + 1;
        int max_key = (trial % 10 == 0) ? 2 : (trial % 10 == 1 ? 4 : (trial % 10 == 2 ? 16 : 100000));
        std::vector<Item> v(n);
        for (size_t i = 0; i < n; ++i) {
            v[i] = {static_cast<int64_t>(rng() % max_key), static_cast<uint32_t>(i)};
        }
        VBlock::Sort(v.begin(), v.end());
        assert(verify(v));
    }
    std::cout << "  [PASS] Fuzzing (200 random trials with low & high keys)\n";

    // 3. Pointer & raw array overload
    {
        int raw[10] = {9, 2, 5, 1, 8, 3, 7, 4, 6, 0};
        VBlock::Sort(raw, 10, std::less<int>());
        for (int i = 0; i < 10; ++i) assert(raw[i] == i);
    }
    std::cout << "  [PASS] Raw pointer overload\n";

    std::cout << "\nAll V-BlockSort standalone tests PASSED successfully!\n";
    return 0;
}
