#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "vblock_layer0.hpp"
#include "vblock_layer2_bench.hpp"

using namespace VBlock;
using namespace VBlockLayer2;

struct Item {
    int64_t key;
    uint32_t id;

    bool operator<(const Item& other) const {
        return key < other.key;
    }
};

template <typename T, typename Compare>
void SymMergeSort(T* arr, size_t n, Compare comp) {
    if (n <= 16) {
        sort16_straight_unroll(arr, comp);
        return;
    }

    // Step 1: Base micro-block sorting (N=16 blocks)
    size_t full_blocks = n / 16;
    for (size_t b = 0; b < full_blocks; ++b) {
        sort16_straight_unroll(arr + b * 16, comp);
    }
    size_t rem = n % 16;
    if (rem > 1) {
        T* tail = arr + full_blocks * 16;
        for (size_t i = 1; i < rem; ++i) {
            T key = tail[i];
            size_t j = i;
            while (j > 0 && comp(key, tail[j - 1])) {
                tail[j] = tail[j - 1];
                --j;
            }
            tail[j] = key;
        }
    }

    // Step 2: Bottom-up SymMerge
    size_t run_len = 16;
    while (run_len < n) {
        size_t double_run = run_len * 2;
        for (size_t i = 0; i < n; i += double_run) {
            if (i + run_len < n) {
                size_t len_a = run_len;
                size_t len_b = std::min(run_len, n - (i + run_len));
                MergeLowKey_SymMerge(arr + i, 0, len_a, len_a + len_b, comp);
            }
        }
        run_len = double_run;
    }
}

bool verify_sorted_and_stable(const std::vector<Item>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) return false;
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) return false;
    }
    return true;
}

int main() {
    const size_t N = 100000;
    std::mt19937 rng(42);

    struct Scenario {
        std::string name;
        int unique_keys;
    };

    std::vector<Scenario> scenarios = {
        {"Binary Keys (2 Keys)", 2},
        {"4 Keys", 4},
        {"16 Keys", 16},
        {"Sqrt(N) Keys (~316 Keys)", 316},
        {"Random Uniform (100k Keys)", 1000000}
    };

    std::cout << "\n=== SYMMERGESORT BENCHMARK (Strict O(1) Memory, In-Place, Stable) ===\n";
    std::cout << std::left  << std::setw(30) << "Distribution"
              << std::right << std::setw(12) << "Time (ms)"
              << std::right << std::setw(16) << "Compares"
              << std::right << std::setw(18) << "Moves"
              << std::right << std::setw(14) << "Status"
              << "\n" << std::string(90, '-') << "\n";

    for (const auto& sc : scenarios) {
        std::vector<Item> data(N);
        for (size_t i = 0; i < N; ++i) {
            data[i] = {static_cast<int64_t>(rng() % sc.unique_keys), static_cast<uint32_t>(i)};
        }

        g_metrics.reset();
        auto start = std::chrono::high_resolution_clock::now();
        SymMergeSort(data.data(), N, [](const Item& a, const Item& b) { return a.key < b.key; });
        auto end = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        bool ok = verify_sorted_and_stable(data);

        std::cout << std::left  << std::setw(30) << sc.name
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2) << ms
                  << std::right << std::setw(16) << g_metrics.comparisons
                  << std::right << std::setw(18) << g_metrics.element_moves
                  << std::right << std::setw(14) << (ok ? "PASS (STABLE)" : "FAIL")
                  << "\n";
    }

    return 0;
}
