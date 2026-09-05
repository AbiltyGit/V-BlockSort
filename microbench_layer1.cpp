#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cassert>
#include "vblock_layer1_bench.hpp"

using namespace VBlockLayer1;

struct Item {
    int32_t key;
    uint32_t id;

    bool operator<(const Item& other) const {
        return key < other.key;
    }
};

// =========================================================================
// EXPERIMENT 1: SEARCH STRATEGY BENCHMARK
// =========================================================================
void run_search_ablation() {
    std::cout << "\n========================================================================\n"
              << "       EXPERIMENT 1: SEARCH STRATEGY BENCHMARK (LINEAR vs BINARY vs GALLOP)\n"
              << "========================================================================\n";

    std::mt19937 rng(42);
    const size_t NUM_QUERIES = 200000;
    std::vector<size_t> test_sizes = {16, 64, 256, 1024};

    for (size_t range_size : test_sizes) {
        std::vector<int> sorted_array(range_size);
        for (size_t i = 0; i < range_size; ++i) sorted_array[i] = static_cast<int>(i * 2);

        // Scenarios: Early target, Random target, Late target
        struct Scenario {
            std::string name;
            std::vector<int> queries;
        };

        std::vector<Scenario> scenarios = {
            {"Early Target (~5% Index)", {}},
            {"Random Target (Uniform)",  {}},
            {"Late Target (~95% Index)",   {}}
        };

        for (size_t q = 0; q < NUM_QUERIES; ++q) {
            // Early
            int early_idx = std::min(range_size - 1, size_t(range_size * 0.05));
            scenarios[0].queries.push_back(sorted_array[early_idx]);

            // Random
            int rand_idx = rng() % range_size;
            scenarios[1].queries.push_back(sorted_array[rand_idx]);

            // Late
            int late_idx = std::min(range_size - 1, size_t(range_size * 0.95));
            scenarios[2].queries.push_back(sorted_array[late_idx]);
        }

        std::cout << "\n\033[1m\033[36m>>> Range Size N = " << range_size 
                  << " (200,000 Queries)\033[0m\n";

        for (const auto& sc : scenarios) {
            std::cout << "\n  [Scenario: " << sc.name << "]\n";
            std::cout << "  " << std::left  << std::setw(20) << "Search Method"
                      << std::right << std::setw(14) << "Time (ms)"
                      << std::right << std::setw(15) << "Latency (ns)"
                      << std::right << std::setw(16) << "Total Compares"
                      << "\n  " << std::string(65, '-') << "\n";

            auto test_method = [&](const std::string& name, auto search_func) {
                g_metrics.reset();
                volatile size_t sink = 0;

                auto start = std::chrono::high_resolution_clock::now();
                for (int val : sc.queries) {
                    auto it = search_func(sorted_array.begin(), sorted_array.end(), val, std::less<int>());
                    sink += std::distance(sorted_array.begin(), it);
                }
                auto end = std::chrono::high_resolution_clock::now();

                double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
                double ns_per_query = (duration_ms * 1e6) / NUM_QUERIES;

                std::cout << "  " << std::left  << std::setw(20) << name
                          << std::right << std::setw(14) << std::fixed << std::setprecision(2) << duration_ms
                          << std::right << std::setw(15) << std::fixed << std::setprecision(1) << ns_per_query
                          << std::right << std::setw(16) << g_metrics.comparisons
                          << "\n";
            };

            test_method("SearchLinear", [](auto first, auto last, int v, auto comp) {
                return SearchLinear(first, last, v, comp);
            });
            test_method("SearchBinary", [](auto first, auto last, int v, auto comp) {
                return SearchBinary(first, last, v, comp);
            });
            test_method("SearchGallop", [](auto first, auto last, int v, auto comp) {
                return SearchGallop(first, last, v, comp, 4);
            });
        }
    }
}

// =========================================================================
// EXPERIMENT 2: BLOCK MOVEMENT STRATEGY BENCHMARK
// =========================================================================
void run_block_movement_ablation() {
    std::cout << "\n========================================================================\n"
              << "       EXPERIMENT 2: BLOCK MOVEMENT (ROTATION vs BLOCK-SELECT vs BLOCK-CYCLE)\n"
              << "========================================================================\n";

    std::mt19937 rng(1337);
    const size_t NUM_TRIALS = 10000;
    const size_t NUM_BLOCKS = 32;   // 32 blocks
    const size_t BLOCK_LEN  = 32;   // 32 elements per block = 1024 elements per trial

    std::cout << "\n\033[1m\033[36m>>> Settings: " << NUM_BLOCKS << " Blocks x " 
              << BLOCK_LEN << " Elements/Block = " << (NUM_BLOCKS * BLOCK_LEN) 
              << " Elements (" << NUM_TRIALS << " Permutations)\033[0m\n\n";

    // Pre-generate permutations
    std::vector<std::vector<size_t>> perms(NUM_TRIALS, std::vector<size_t>(NUM_BLOCKS));
    for (size_t t = 0; t < NUM_TRIALS; ++t) {
        std::iota(perms[t].begin(), perms[t].end(), 0);
        std::shuffle(perms[t].begin(), perms[t].end(), rng);
    }

    std::vector<int> master_arr(NUM_BLOCKS * BLOCK_LEN);
    for (size_t i = 0; i < master_arr.size(); ++i) master_arr[i] = static_cast<int>(i);

    std::cout << std::left  << std::setw(25) << "Movement Method"
              << std::right << std::setw(14) << "Time (ms)"
              << std::right << std::setw(16) << "Latency (us/perm)"
              << std::right << std::setw(18) << "Elements Moved"
              << std::right << std::setw(16) << "Compares"
              << "\n" << std::string(89, '-') << "\n";

    // 1. Rotation-based Rolling
    {
        g_metrics.reset();
        std::vector<int> arr = master_arr;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t t = 0; t < NUM_TRIALS; ++t) {
            BlockMoveRotation(arr.data(), NUM_BLOCKS, BLOCK_LEN, perms[t], std::less<int>());
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << std::left  << std::setw(25) << "BlockMoveRotation"
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2) << ms
                  << std::right << std::setw(16) << std::fixed << std::setprecision(2) << (ms * 1e3 / NUM_TRIALS)
                  << std::right << std::setw(18) << g_metrics.element_moves
                  << std::right << std::setw(16) << g_metrics.comparisons
                  << "\n";
    }

    // 2. Selection Sort on Blocks (KotaSort blockSelect)
    {
        g_metrics.reset();
        std::vector<int> arr = master_arr;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t t = 0; t < NUM_TRIALS; ++t) {
            std::vector<size_t> tags = perms[t];
            BlockMoveSelect(arr.data(), NUM_BLOCKS, BLOCK_LEN, tags, std::less<int>());
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << std::left  << std::setw(25) << "BlockMoveSelect"
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2) << ms
                  << std::right << std::setw(16) << std::fixed << std::setprecision(2) << (ms * 1e3 / NUM_TRIALS)
                  << std::right << std::setw(18) << g_metrics.element_moves
                  << std::right << std::setw(16) << g_metrics.comparisons
                  << "\n";
    }

    // 3. Cycle Sort on Blocks (KotaSort blockCycle)
    {
        g_metrics.reset();
        std::vector<int> arr = master_arr;
        std::vector<int> aux_buf(BLOCK_LEN);
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t t = 0; t < NUM_TRIALS; ++t) {
            std::vector<size_t> tags = perms[t];
            BlockMoveCycle(arr.data(), NUM_BLOCKS, BLOCK_LEN, tags, aux_buf.data(), std::less<int>());
        }
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << std::left  << std::setw(25) << "BlockMoveCycle"
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2) << ms
                  << std::right << std::setw(16) << std::fixed << std::setprecision(2) << (ms * 1e3 / NUM_TRIALS)
                  << std::right << std::setw(18) << g_metrics.element_moves
                  << std::right << std::setw(16) << g_metrics.comparisons
                  << "\n";
    }
}

// =========================================================================
// EXPERIMENT 3: ADAPTIVE BOUNDARY SKIP BENCHMARK
// =========================================================================
void run_boundary_ablation() {
    std::cout << "\n========================================================================\n"
              << "       EXPERIMENT 3: ADAPTIVE BOUNDARY SKIP EFFICIENCY\n"
              << "========================================================================\n";

    const size_t NUM_PAIRS = 500000;
    std::vector<int> A(32), B(32);

    // Scenario A: Already sorted (A < B)
    for (int i = 0; i < 32; ++i) { A[i] = i; B[i] = 32 + i; }
    
    g_metrics.reset();
    size_t skips = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t p = 0; p < NUM_PAIRS; ++p) {
        if (CheckBoundaries(A.data(), A.data() + 32, B.data(), B.data() + 32, std::less<int>()) == BoundaryResult::ALREADY_MERGED) {
            skips++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "500,000 Sorted Pair Boundary Checks: " << ms << " ms (" 
              << skips << " skips, " << g_metrics.comparisons << " comparisons)\n"
              << "Average cost per boundary check: " << (ms * 1e6 / NUM_PAIRS) << " ns (EXACTLY 1 compare!)\n";
}

int main() {
    run_search_ablation();
    run_block_movement_ablation();
    run_boundary_ablation();
    std::cout << "\nAll ablation studies completed.\n";
    return 0;
}
