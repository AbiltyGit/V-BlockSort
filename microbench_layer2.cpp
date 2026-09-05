#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "vblock_layer2_bench.hpp"

using namespace VBlockLayer2;

struct Item {
    int64_t key;
    uint32_t id;

    bool operator<(const Item& other) const {
        return key < other.key;
    }
};

bool verify_sorted_and_stable(const std::vector<Item>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) return false;
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) return false;
    }
    return true;
}

// =========================================================================
// EXPERIMENT 1: KEY EXTRACTION STRATEGY BENCHMARK
// =========================================================================
void run_key_extraction_ablation() {
    std::cout << "\n========================================================================\n"
              << "       EXPERIMENT 1: BUFFER KEY EXTRACTION (SCAN-ROTATE vs GALLOP-PULL vs PROBE)\n"
              << "========================================================================\n";

    const size_t N = 100000;
    const size_t TARGET_KEYS = static_cast<size_t>(2 * std::sqrt(N)); // 632 keys

    std::mt19937 rng(42);

    struct Scenario {
        std::string name;
        std::vector<Item> data;
    };

    std::vector<Scenario> scenarios = {
        {"Random Uniform (Abundant Keys)", {}},
        {"Sqrt(N) Keys (Borderline Keys)",  {}},
        {"Binary Keys (Extreme Low-Key)",  {}},
        {"Already Sorted",                 {}}
    };

    // Populate data
    scenarios[0].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[0].data[i] = {static_cast<int64_t>(rng() % 1000000), static_cast<uint32_t>(i)};

    scenarios[1].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[1].data[i] = {static_cast<int64_t>(rng() % 316), static_cast<uint32_t>(i)};

    scenarios[2].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[2].data[i] = {static_cast<int64_t>(rng() % 2), static_cast<uint32_t>(i)};

    scenarios[3].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[3].data[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};

    for (const auto& sc : scenarios) {
        std::cout << "\n\033[1m\033[36m>>> Scenario: " << sc.name << " (Target: " << TARGET_KEYS << " Keys)\033[0m\n";
        std::cout << std::left  << std::setw(28) << "Extraction Method"
                  << std::right << std::setw(12) << "Time (ms)"
                  << std::right << std::setw(16) << "Compares"
                  << std::right << std::setw(18) << "Elements Moved"
                  << std::right << std::setw(15) << "Keys Found"
                  << "\n" << std::string(89, '-') << "\n";

        auto test_method = [&](const std::string& name, auto extract_func) {
            std::vector<Item> arr = sc.data;
            g_metrics.reset();

            auto start = std::chrono::high_resolution_clock::now();
            size_t found = extract_func(arr.data(), N, TARGET_KEYS, [](const Item& a, const Item& b) { return a.key < b.key; });
            auto end = std::chrono::high_resolution_clock::now();

            double ms = std::chrono::duration<double, std::milli>(end - start).count();

            std::cout << std::left  << std::setw(28) << name
                      << std::right << std::setw(12) << std::fixed << std::setprecision(2) << ms
                      << std::right << std::setw(16) << g_metrics.comparisons
                      << std::right << std::setw(18) << g_metrics.element_moves
                      << std::right << std::setw(15) << found
                      << "\n";
        };

        test_method("ExtractKeys_ScanRotate", [](Item* arr, size_t n, size_t k, auto comp) {
            return ExtractKeys_ScanRotate(arr, n, k, comp);
        });
        test_method("ExtractKeys_GallopingPull", [](Item* arr, size_t n, size_t k, auto comp) {
            return ExtractKeys_GallopingPull(arr, n, k, comp);
        });
        test_method("ExtractKeys_AdaptiveBudget", [](Item* arr, size_t n, size_t k, auto comp) {
            return ExtractKeys_AdaptiveBudget(arr, n, k, comp);
        });
    }
}

// =========================================================================
// EXPERIMENT 2: LOW-KEY MERGE SHIELD BENCHMARK
// =========================================================================
void run_lowkey_merge_ablation() {
    std::cout << "\n========================================================================\n"
              << "       EXPERIMENT 2: LOW-KEY MERGE SHIELD (KOTA-FALLBACK vs HWANG-LIN vs RUN-LENGTH)\n"
              << "========================================================================\n";

    const size_t N = 100000;
    const size_t MID = N / 2; // Two halves of 50,000 elements each
    std::mt19937 rng(1337);

    struct Scenario {
        std::string name;
        int unique_keys;
    };

    std::vector<Scenario> scenarios = {
        {"Binary Keys (2 Keys)", 2},
        {"4 Keys", 4},
        {"16 Keys", 16},
        {"Sqrt(N) Keys (~316 Keys)", 316}
    };

    for (const auto& sc : scenarios) {
        std::cout << "\n\033[1m\033[36m>>> Merging Two 50k Sorted Halves (" << sc.name << ")\033[0m\n";
        std::cout << std::left  << std::setw(30) << "Low-Key Merge Method"
                  << std::right << std::setw(12) << "Time (ms)"
                  << std::right << std::setw(16) << "Compares"
                  << std::right << std::setw(18) << "Elements Moved"
                  << std::right << std::setw(14) << "Status"
                  << "\n" << std::string(90, '-') << "\n";

        // Generate two individually sorted halves
        std::vector<Item> master(N);
        std::vector<int64_t> half1(MID), half2(N - MID);
        for (size_t i = 0; i < MID; ++i) half1[i] = rng() % sc.unique_keys;
        for (size_t i = 0; i < N - MID; ++i) half2[i] = rng() % sc.unique_keys;
        std::sort(half1.begin(), half1.end());
        std::sort(half2.begin(), half2.end());

        for (size_t i = 0; i < MID; ++i) master[i] = {half1[i], static_cast<uint32_t>(i)};
        for (size_t i = 0; i < N - MID; ++i) master[MID + i] = {half2[i], static_cast<uint32_t>(MID + i)};

        auto test_merge = [&](const std::string& name, auto merge_func) {
            std::vector<Item> arr = master;
            g_metrics.reset();

            auto start = std::chrono::high_resolution_clock::now();
            merge_func(arr.data(), 0, MID, N, [](const Item& a, const Item& b) { return a.key < b.key; });
            auto end = std::chrono::high_resolution_clock::now();

            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            bool ok = verify_sorted_and_stable(arr);

            std::cout << std::left  << std::setw(30) << name
                      << std::right << std::setw(12) << std::fixed << std::setprecision(2) << ms
                      << std::right << std::setw(16) << g_metrics.comparisons
                      << std::right << std::setw(18) << g_metrics.element_moves
                      << std::right << std::setw(14) << (ok ? "\033[32mPASS (STABLE)\033[0m" : "\033[31mFAIL\033[0m")
                      << "\n";
        };

        // If sc.unique_keys is large, KotaFallback can take 10+ seconds, run with timeout protection or smaller limit
        if (sc.unique_keys <= 4) {
            test_merge("MergeLowKey_KotaFallback", [](Item* arr, size_t a, size_t m, size_t b, auto comp) {
                MergeLowKey_KotaFallback(arr, a, m, b, comp);
            });
        } else {
            std::cout << std::left  << std::setw(30) << "MergeLowKey_KotaFallback"
                      << std::right << std::setw(12) << "SKIPPED"
                      << std::right << std::setw(16) << "(O(N^2) explosion)"
                      << std::right << std::setw(18) << "-"
                      << std::right << std::setw(14) << "-"
                      << "\n";
        }

        test_merge("MergeLowKey_SymMerge", [](Item* arr, size_t a, size_t m, size_t b, auto comp) {
            MergeLowKey_SymMerge(arr, a, m, b, comp);
        });

        test_merge("MergeLowKey_RunLengthGallop", [](Item* arr, size_t a, size_t m, size_t b, auto comp) {
            MergeLowKey_RunLengthGallop(arr, a, m, b, comp);
        });
    }
}

int main() {
    run_key_extraction_ablation();
    run_lowkey_merge_ablation();
    std::cout << "\nLayer 2 ablation complete.\n";
    return 0;
}
