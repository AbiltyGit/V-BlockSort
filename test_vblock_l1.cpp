#include <iostream>
#include <vector>
#include <random>
#include <cassert>
#include "vblock_layer1.hpp"

struct Item {
    int64_t key;
    uint32_t id;

    bool operator<(const Item& other) const {
        return key < other.key;
    }
};

bool verify(const std::vector<Item>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) {
            std::cerr << "NOT SORTED at " << i << ": " << arr[i - 1].key << " > " << arr[i].key << "\n";
            return false;
        }
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) {
            std::cerr << "NOT STABLE at " << i << ": key=" << arr[i].key 
                      << ", prev_id=" << arr[i - 1].id << " > curr_id=" << arr[i].id << "\n";
            return false;
        }
    }
    return true;
}

int main() {
    std::mt19937 rng(42);
    std::vector<size_t> sizes = {0, 1, 2, 3, 15, 16, 17, 31, 32, 64, 100, 128, 256, 512, 1000, 5000, 20000, 100000};

    std::cout << "Testing VBlock::SortLayer1 across " << sizes.size() << " sizes...\n";

    for (size_t n : sizes) {
        // Test 1: Random
        {
            std::vector<Item> v(n);
            for (size_t i = 0; i < n; ++i) v[i] = {static_cast<int64_t>(rng() % (n > 0 ? n : 1)), static_cast<uint32_t>(i)};
            VBlock::SortLayer1(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }

        // Test 2: Binary keys
        {
            std::vector<Item> v(n);
            for (size_t i = 0; i < n; ++i) v[i] = {static_cast<int64_t>(rng() % 2), static_cast<uint32_t>(i)};
            VBlock::SortLayer1(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }

        // Test 3: All equal
        {
            std::vector<Item> v(n);
            for (size_t i = 0; i < n; ++i) v[i] = {42, static_cast<uint32_t>(i)};
            VBlock::SortLayer1(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }

        // Test 4: Reverse
        {
            std::vector<Item> v(n);
            for (size_t i = 0; i < n; ++i) v[i] = {static_cast<int64_t>(n - i), static_cast<uint32_t>(i)};
            VBlock::SortLayer1(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }

        // Test 5: Sorted
        {
            std::vector<Item> v(n);
            for (size_t i = 0; i < n; ++i) v[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            VBlock::SortLayer1(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }
    }

    std::cout << "All VBlock::SortLayer1 verification tests PASSED!\n";
    return 0;
}
