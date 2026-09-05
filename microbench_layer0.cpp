#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "vblock_layer0.hpp"

struct Item {
    int32_t key;
    uint32_t id;

    bool operator<(const Item& other) const {
        return key < other.key;
    }
};

bool verify_sorted_and_stable(const Item* arr, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        if (arr[i].key < arr[i - 1].key) return false;
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) return false;
    }
    return true;
}

// Microbenchmark Runner
template <typename Func>
double benchmark_blocks(const std::vector<std::vector<Item>>& datasets, Func sort_func) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Each thread/run works on copies of datasets
    for (const auto& block : datasets) {
        Item local_block[16];
        for (int i = 0; i < 16; ++i) local_block[i] = block[i];
        
        sort_func(local_block);
        
        // Prevent dead-code elimination by compiler
        asm volatile("" : : "r"(local_block) : "memory");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count();
}

int main() {
    std::cout << "\n========================================================================\n"
              << "       V-BLOCKSORT LAYER 0: N=16 MICRO-KERNEL VALIDATION & BENCHMARK\n"
              << "========================================================================\n\n";

    // 1. Correctness & Stability Validation Suite
    std::mt19937 rng(42);
    const int NUM_VALIDATION_TESTS = 10000;

    auto validate = [&](const std::string& name, auto sort_func) {
        bool all_ok = true;
        for (int t = 0; t < NUM_VALIDATION_TESTS; ++t) {
            Item arr[16];
            int dist = t % 5;
            for (int i = 0; i < 16; ++i) {
                int key = 0;
                if (dist == 0) key = rng() % 1000;      // random
                else if (dist == 1) key = rng() % 2;    // binary 2-keys (high collision)
                else if (dist == 2) key = 42;           // all equal
                else if (dist == 3) key = 16 - i;       // reversed
                else if (dist == 4) key = i;            // sorted
                arr[i] = {key, static_cast<uint32_t>(i)};
            }

            sort_func(arr);

            if (!verify_sorted_and_stable(arr, 16)) {
                all_ok = false;
                break;
            }
        }
        std::cout << std::left << std::setw(32) << name << " : "
                  << (all_ok ? "\033[32mPASS (100% SORTED & STABLE)\033[0m" : "\033[31mFAIL (UNSTABLE OR NOT SORTED)\033[0m")
                  << "\n";
        return all_ok;
    };

    std::cout << "--- Stability & Correctness Tests (10,000 Random/Adversarial Blocks) ---\n";
    validate("sort16_network_tagged", [](Item* arr) {
        VBlock::sort16_network_tagged(arr, [](const Item& a, const Item& b) { return a.key < b.key; });
    });
    validate("sort16_oddeven_branchless", [](Item* arr) {
        VBlock::sort16_oddeven_branchless(arr, [](const Item& a, const Item& b) { return a.key < b.key; });
    });
    validate("sort16_unrolled_insertion", [](Item* arr) {
        VBlock::sort16_unrolled_insertion(arr, [](const Item& a, const Item& b) { return a.key < b.key; });
    });
    validate("sort16_binary_insertion", [](Item* arr) {
        VBlock::sort16_binary_insertion(arr, [](const Item& a, const Item& b) { return a.key < b.key; });
    });
    validate("std::stable_sort", [](Item* arr) {
        std::stable_sort(arr, arr + 16, [](const Item& a, const Item& b) { return a.key < b.key; });
    });

    std::cout << "\n------------------------------------------------------------------------\n";
    std::cout << "--- Throughput Benchmark (1,000,000 Blocks of 16 Items = 16M Elements) ---\n";
    std::cout << "------------------------------------------------------------------------\n";

    const size_t NUM_BLOCKS = 1000000;

    std::vector<std::string> scenario_names = {
        "Random Uniform",
        "Binary Keys (High Collision)",
        "Already Sorted",
        "Strictly Reversed"
    };

    for (int scen = 0; scen < 4; ++scen) {
        std::cout << "\n\033[1m\033[36m>>> Scenario: " << scenario_names[scen] << "\033[0m\n";
        std::cout << std::left  << std::setw(30) << "Candidate"
                  << std::right << std::setw(15) << "Total Time (ms)"
                  << std::right << std::setw(16) << "Latency (ns/blk)"
                  << std::right << std::setw(18) << "Throughput (M/s)"
                  << "\n" << std::string(80, '-') << "\n";

        std::vector<std::vector<Item>> blocks(NUM_BLOCKS, std::vector<Item>(16));
        for (size_t b = 0; b < NUM_BLOCKS; ++b) {
            for (int i = 0; i < 16; ++i) {
                int key = 0;
                if (scen == 0) key = rng() % 1000000;
                else if (scen == 1) key = rng() % 2;
                else if (scen == 2) key = i;
                else if (scen == 3) key = 16 - i;
                blocks[b][i] = {key, static_cast<uint32_t>(i)};
            }
        }

        auto run_bench = [&](const std::string& name, auto sort_func) {
            double total_ns = benchmark_blocks(blocks, sort_func);
            double total_ms = total_ns / 1e6;
            double ns_per_block = total_ns / NUM_BLOCKS;
            double m_items_sec = (NUM_BLOCKS * 16.0) / (total_ns / 1e3);

            std::cout << std::left  << std::setw(30) << name
                      << std::right << std::setw(15) << std::fixed << std::setprecision(2) << total_ms
                      << std::right << std::setw(16) << std::fixed << std::setprecision(1) << ns_per_block
                      << std::right << std::setw(18) << std::fixed << std::setprecision(1) << m_items_sec
                      << "\n";
        };

        run_bench("sort16_unrolled_insertion", [](Item* arr) {
            VBlock::sort16_unrolled_insertion(arr, [](const Item& a, const Item& b) { return a.key < b.key; });
        });
        run_bench("sort16_straight_unroll", [](Item* arr) {
            VBlock::sort16_straight_unroll(arr, [](const Item& a, const Item& b) { return a.key < b.key; });
        });
        run_bench("sort16_oddeven_branchless", [](Item* arr) {
            VBlock::sort16_oddeven_branchless(arr, [](const Item& a, const Item& b) { return a.key < b.key; });
        });
        run_bench("std::stable_sort", [](Item* arr) {
            std::stable_sort(arr, arr + 16, [](const Item& a, const Item& b) { return a.key < b.key; });
        });
    }

    std::cout << "\nBenchmark complete.\n";
    return 0;
}
