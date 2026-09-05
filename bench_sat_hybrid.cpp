#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cassert>

#include "vblock_sort.hpp"
#include "vblock_sort_sat_hybrid.hpp"

struct Elem {
    int32_t val;
    int32_t id;

    bool operator<(const Elem& o) const {
        return val < o.val;
    }
};

enum class Distribution {
    RANDOM,
    TWO_KEYS,
    FOUR_KEYS,
    SQRT_KEYS,
    ALL_EQUAL,
    SORTED,
    REVERSED,
    ALMOST_SORTED,
    ORGAN_PIPE,
    REVERSE_ORGAN_PIPE,
    SAWTOOTH,
    ROTATED
};

std::string DistName(Distribution d) {
    switch (d) {
        case Distribution::RANDOM: return "Random Uniform";
        case Distribution::TWO_KEYS: return "Binary (2 Keys)";
        case Distribution::FOUR_KEYS: return "4 Keys";
        case Distribution::SQRT_KEYS: return "sqrt(N) Keys (Kota Trap)";
        case Distribution::ALL_EQUAL: return "All Equal (1 Key)";
        case Distribution::SORTED: return "Already Sorted";
        case Distribution::REVERSED: return "Strictly Reversed";
        case Distribution::ALMOST_SORTED: return "Almost Sorted (95%)";
        case Distribution::ORGAN_PIPE: return "Organ Pipe (Bitonic)";
        case Distribution::REVERSE_ORGAN_PIPE: return "Reverse Organ Pipe";
        case Distribution::SAWTOOTH: return "Sawtooth (16 Ramps)";
        case Distribution::ROTATED: return "Rotated Sorted";
    }
    return "Unknown";
}

std::vector<Elem> GenerateData(size_t N, Distribution dist, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Elem> data(N);

    switch (dist) {
        case Distribution::RANDOM:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(rng() % (N * 10)), static_cast<int32_t>(i)};
            break;
        case Distribution::TWO_KEYS:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(rng() % 2), static_cast<int32_t>(i)};
            break;
        case Distribution::FOUR_KEYS:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(rng() % 4), static_cast<int32_t>(i)};
            break;
        case Distribution::SQRT_KEYS: {
            int k = std::max(2, static_cast<int>(std::sqrt(N)));
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(rng() % k), static_cast<int32_t>(i)};
            break;
        }
        case Distribution::ALL_EQUAL:
            for (size_t i = 0; i < N; ++i) data[i] = {42, static_cast<int32_t>(i)};
            break;
        case Distribution::SORTED:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(i), static_cast<int32_t>(i)};
            break;
        case Distribution::REVERSED:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(N - i), static_cast<int32_t>(i)};
            break;
        case Distribution::ALMOST_SORTED:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(i), static_cast<int32_t>(i)};
            for (size_t i = 0; i < N / 20; ++i) {
                size_t idx1 = rng() % N;
                size_t idx2 = rng() % N;
                std::swap(data[idx1], data[idx2]);
            }
            break;
        case Distribution::ORGAN_PIPE: {
            size_t mid = N / 2;
            for (size_t i = 0; i < mid; ++i) data[i] = {static_cast<int32_t>(i), static_cast<int32_t>(i)};
            for (size_t i = mid; i < N; ++i) data[i] = {static_cast<int32_t>(N - i), static_cast<int32_t>(i)};
            break;
        }
        case Distribution::REVERSE_ORGAN_PIPE: {
            size_t mid = N / 2;
            for (size_t i = 0; i < mid; ++i) data[i] = {static_cast<int32_t>(mid - i), static_cast<int32_t>(i)};
            for (size_t i = mid; i < N; ++i) data[i] = {static_cast<int32_t>(i - mid), static_cast<int32_t>(i)};
            break;
        }
        case Distribution::SAWTOOTH: {
            size_t ramp_len = N / 16;
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(i % ramp_len), static_cast<int32_t>(i)};
            break;
        }
        case Distribution::ROTATED: {
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(i), static_cast<int32_t>(i)};
            std::rotate(data.begin(), data.begin() + N / 3, data.end());
            break;
        }
    }
    return data;
}

bool CheckStable(const std::vector<Elem>& original, const std::vector<Elem>& sorted_arr) {
    for (size_t i = 1; i < sorted_arr.size(); ++i) {
        if (sorted_arr[i].val < sorted_arr[i - 1].val) return false;
        if (sorted_arr[i].val == sorted_arr[i - 1].val && sorted_arr[i].id < sorted_arr[i - 1].id) return false;
    }
    return true;
}

void RunBenchSuite(size_t N, int RUNS) {
    std::cout << "\n=========================================================================================\n";
    std::cout << "         BENCHMARK: V-BLOCKSORT ORIGINAL vs V-BLOCKSORT SAT-HYBRID (N = " << N << ")    \n";
    std::cout << "=========================================================================================\n\n";

    std::vector<Distribution> distributions = {
        Distribution::RANDOM,
        Distribution::TWO_KEYS,
        Distribution::FOUR_KEYS,
        Distribution::SQRT_KEYS,
        Distribution::ALL_EQUAL,
        Distribution::SORTED,
        Distribution::REVERSED,
        Distribution::ALMOST_SORTED,
        Distribution::ORGAN_PIPE,
        Distribution::REVERSE_ORGAN_PIPE,
        Distribution::SAWTOOTH,
        Distribution::ROTATED
    };

    std::cout << std::left << std::setw(28) << "Distribution"
              << std::right << std::setw(18) << "Original V-Block"
              << std::setw(20) << "SAT-Hybrid V-Block"
              << std::setw(15) << "Speedup / Diff"
              << std::setw(12) << "Stability"
              << "\n";
    std::cout << std::string(93, '-') << "\n";

    for (auto dist : distributions) {
        double total_orig = 0;
        double total_sat = 0;
        bool all_stable = true;

        for (int r = 0; r < RUNS; ++r) {
            auto data1 = GenerateData(N, dist, 1000 + r * 17);
            auto data2 = data1;

            auto t0 = std::chrono::high_resolution_clock::now();
            VBlock::Sort(data1.begin(), data1.end(), [](const Elem& a, const Elem& b) { return a.val < b.val; });
            auto t1 = std::chrono::high_resolution_clock::now();
            total_orig += std::chrono::duration<double, std::milli>(t1 - t0).count();

            auto t2 = std::chrono::high_resolution_clock::now();
            VBlockSat::Sort(data2.begin(), data2.end(), [](const Elem& a, const Elem& b) { return a.val < b.val; });
            auto t3 = std::chrono::high_resolution_clock::now();
            total_sat += std::chrono::duration<double, std::milli>(t3 - t2).count();

            if (!CheckStable(data1, data2)) {
                all_stable = false;
            }
        }

        double avg_orig = total_orig / RUNS;
        double avg_sat = total_sat / RUNS;
        double speedup = avg_orig / avg_sat;

        std::cout << std::left << std::setw(28) << DistName(dist)
                  << std::right << std::setw(15) << std::fixed << std::setprecision(2) << avg_orig << " ms"
                  << std::setw(17) << avg_sat << " ms"
                  << std::setw(13) << std::setprecision(2) << speedup << "x"
                  << std::setw(12) << (all_stable ? "100% OK" : "FAIL")
                  << "\n";
    }

    std::cout << std::string(93, '-') << "\n";
}

int main() {
    RunBenchSuite(100000, 3);
    RunBenchSuite(1000000, 3);
    return 0;
}
