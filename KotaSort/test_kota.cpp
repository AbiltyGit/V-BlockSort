#include "kota_clean.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <cassert>

struct Item {
    int key;
    int id;

    bool operator<(const Item& other) const {
        return key < other.key;
    }
};

bool verify(const std::vector<Item>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) {
            std::cerr << "NOT SORTED at index " << i << ": " << arr[i - 1].key << " > " << arr[i].key << "\n";
            return false;
        }
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) {
            std::cerr << "NOT STABLE at index " << i << ": key=" << arr[i].key << ", id=" << arr[i - 1].id << " before " << arr[i].id << "\n";
            return false;
        }
    }
    return true;
}

int main() {
    std::mt19937 rng(1337);

    std::vector<int> sizes = {0, 1, 2, 3, 4, 10, 31, 64, 127, 128, 129, 256, 500, 1024, 5000, 20000};
    
    for (int n : sizes) {
        std::cout << "Testing size: " << n << std::endl;
        // Test 1: Random integers
        {
            std::vector<Item> v(n);
            for (int i = 0; i < n; ++i) {
                v[i] = {static_cast<int>(rng() % (n > 0 ? n : 1)), i};
            }
            KotaClean::sort(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            if (!verify(v)) {
                std::cout << "Failed on random size " << n << std::endl;
                return 1;
            }
        }

        // Test 2: Few unique keys (2 unique keys: 0 and 1)
        {
            std::vector<Item> v(n);
            for (int i = 0; i < n; ++i) {
                v[i] = {static_cast<int>(rng() % 2), i};
            }
            KotaClean::sort(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }

        // Test 3: All equal
        {
            std::vector<Item> v(n);
            for (int i = 0; i < n; ++i) {
                v[i] = {42, i};
            }
            KotaClean::sort(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }

        // Test 4: Reverse sorted
        {
            std::vector<Item> v(n);
            for (int i = 0; i < n; ++i) {
                v[i] = {n - i, i};
            }
            KotaClean::sort(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }

        // Test 5: Already sorted
        {
            std::vector<Item> v(n);
            for (int i = 0; i < n; ++i) {
                v[i] = {i, i};
            }
            KotaClean::sort(v.data(), v.size(), [](const Item& a, const Item& b) { return a.key < b.key; });
            assert(verify(v));
        }
    }

    std::cout << "All KotaSort verification tests PASSED!\n";
    return 0;
}
