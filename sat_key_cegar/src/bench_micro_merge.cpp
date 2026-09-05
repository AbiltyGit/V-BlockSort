#include "../include/synthesized_invariants.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>

struct Record {
    int key;
    int id;

    bool operator<(const Record& o) const {
        return key < o.key;
    }
};

int main() {
    std::cout << "=========================================================\n";
    std::cout << " MICRO-BENCHMARK: SYNTHESIZED FLAT MERGE vs STD::INPLACE \n";
    std::cout << "=========================================================\n\n";

    const int ITERS = 2000000;
    std::mt19937_64 rng(12345);

    auto comp = [](const Record& a, const Record& b) {
        return a.key < b.key;
    };

    // Benchmark 3+3 Merge across 2 Million runs
    std::vector<Record> pool_synth(ITERS * 6);
    std::vector<Record> pool_std(ITERS * 6);

    for (int i = 0; i < ITERS; ++i) {
        std::vector<Record> r = {
            {static_cast<int>(rng() % 100), 0},
            {static_cast<int>(rng() % 100), 1},
            {static_cast<int>(rng() % 100), 2},
            {static_cast<int>(rng() % 100), 3},
            {static_cast<int>(rng() % 100), 4},
            {static_cast<int>(rng() % 100), 5}
        };
        std::stable_sort(r.begin(), r.begin() + 3, comp);
        std::stable_sort(r.begin() + 3, r.end(), comp);

        for (int j = 0; j < 6; ++j) {
            pool_synth[i * 6 + j] = r[j];
            pool_std[i * 6 + j] = r[j];
        }
    }

    // 1. Benchmark Synthesized 3+3 Kernel
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERS; ++i) {
        SatCegar::Synthesized::merge_3_3_stable(&pool_synth[i * 6], comp);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double time_synth = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 2. Benchmark std::inplace_merge
    auto t2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERS; ++i) {
        std::inplace_merge(&pool_std[i * 6], &pool_std[i * 6 + 3], &pool_std[i * 6 + 6], comp);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    double time_std = std::chrono::duration<double, std::milli>(t3 - t2).count();

    std::cout << "3+3 Merge (" << ITERS << " iterations):\n";
    std::cout << "  - std::inplace_merge        : " << std::setw(8) << time_std << " ms\n";
    std::cout << "  - Synthesized SAT Micro-Merge: " << std::setw(8) << time_synth << " ms\n";
    std::cout << "  -> Speedup: " << std::fixed << std::setprecision(2) << (time_std / time_synth) << "x FASTER!\n\n";

    return 0;
}
