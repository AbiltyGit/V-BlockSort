#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cassert>

#include "vblock_sort.hpp"

struct Elem {
    int32_t val;
    int32_t id;

    bool operator<(const Elem& o) const {
        return val < o.val;
    }
};

enum class DistType {
    RANDOM,
    SQRT_KEYS,
    ALMOST_SORTED,
    ORGAN_PIPE
};

std::string DistName(DistType d) {
    switch (d) {
        case DistType::RANDOM: return "Random Uniform";
        case DistType::SQRT_KEYS: return "sqrt(N) Keys (Low Cardinality)";
        case DistType::ALMOST_SORTED: return "Almost Sorted (95%)";
        case DistType::ORGAN_PIPE: return "Organ Pipe (Bitonic)";
    }
    return "Unknown";
}

std::vector<Elem> GenerateData(size_t N, DistType dist, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Elem> data(N);

    switch (dist) {
        case DistType::RANDOM:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(rng() % (N * 10)), static_cast<int32_t>(i)};
            break;
        case DistType::SQRT_KEYS: {
            int k = std::max(2, static_cast<int>(std::sqrt(N)));
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(rng() % k), static_cast<int32_t>(i)};
            break;
        }
        case DistType::ALMOST_SORTED:
            for (size_t i = 0; i < N; ++i) data[i] = {static_cast<int32_t>(i), static_cast<int32_t>(i)};
            for (size_t i = 0; i < N / 20; ++i) {
                size_t idx1 = rng() % N;
                size_t idx2 = rng() % N;
                std::swap(data[idx1], data[idx2]);
            }
            break;
        case DistType::ORGAN_PIPE: {
            size_t mid = N / 2;
            for (size_t i = 0; i < mid; ++i) data[i] = {static_cast<int32_t>(i), static_cast<int32_t>(i)};
            for (size_t i = mid; i < N; ++i) data[i] = {static_cast<int32_t>(N - i), static_cast<int32_t>(i)};
            break;
        }
    }
    return data;
}

bool CheckStable(const std::vector<Elem>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].val < arr[i - 1].val) return false;
        if (arr[i].val == arr[i - 1].val && arr[i].id < arr[i - 1].id) return false;
    }
    return true;
}

void RunHighScaleTest(size_t N) {
    std::cout << "\n=========================================================================================\n";
    std::cout << " HIGH-SCALE BENCHMARK: N = " << N << " (" << (N / 1000000.0) << " Million Elements / " 
              << (N * sizeof(Elem) / (1024.0 * 1024.0)) << " MB Array)\n";
    std::cout << "=========================================================================================\n\n";

    std::vector<DistType> dists = {
        DistType::RANDOM,
        DistType::SQRT_KEYS,
        DistType::ALMOST_SORTED,
        DistType::ORGAN_PIPE
    };

    std::cout << std::left << std::setw(30) << "Distribution"
              << std::right << std::setw(16) << "std::sort"
              << std::setw(18) << "std::stable_sort"
              << std::setw(18) << "V-Block (4KB Stack)"
              << std::setw(18) << "V-Block (0B SAT)"
              << std::setw(12) << "Ratio"
              << "\n";
    std::cout << std::string(112, '-') << "\n";

    for (auto d : dists) {
        auto master = GenerateData(N, d, 42);

        // 1. std::sort (Unstable)
        auto d_sort = master;
        auto t0 = std::chrono::high_resolution_clock::now();
        std::sort(d_sort.begin(), d_sort.end(), [](const Elem& a, const Elem& b) { return a.val < b.val; });
        auto t1 = std::chrono::high_resolution_clock::now();
        double time_sort = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // 2. std::stable_sort (Heap O(N))
        auto d_stable = master;
        auto t2 = std::chrono::high_resolution_clock::now();
        std::stable_sort(d_stable.begin(), d_stable.end(), [](const Elem& a, const Elem& b) { return a.val < b.val; });
        auto t3 = std::chrono::high_resolution_clock::now();
        double time_stable = std::chrono::duration<double, std::milli>(t3 - t2).count();

        // 3. V-BlockSort (4 KB Stack Buffer)
        auto d_vblock = master;
        auto t4 = std::chrono::high_resolution_clock::now();
        VBlock::Sort<4096>(d_vblock.begin(), d_vblock.end(), [](const Elem& a, const Elem& b) { return a.val < b.val; });
        auto t5 = std::chrono::high_resolution_clock::now();
        double time_vblock = std::chrono::duration<double, std::milli>(t5 - t4).count();

        // 4. V-BlockSort (0 B Stack Buffer - Pure In-Place SAT)
        auto d_sat = master;
        auto t6 = std::chrono::high_resolution_clock::now();
        VBlock::Sort<0>(d_sat.begin(), d_sat.end(), [](const Elem& a, const Elem& b) { return a.val < b.val; });
        auto t7 = std::chrono::high_resolution_clock::now();
        double time_sat = std::chrono::duration<double, std::milli>(t7 - t6).count();

        assert(CheckStable(d_sat));

        double ratio = time_sat / time_vblock;

        std::cout << std::left << std::setw(30) << DistName(d)
                  << std::right << std::setw(13) << std::fixed << std::setprecision(1) << time_sort << " ms"
                  << std::setw(15) << time_stable << " ms"
                  << std::setw(15) << time_vblock << " ms"
                  << std::setw(15) << time_sat << " ms"
                  << std::setw(11) << std::setprecision(2) << ratio << "x"
                  << "\n";
    }
    std::cout << std::string(112, '-') << "\n";
}

int main() {
    RunHighScaleTest(2000000);   // 2 Million Elements
    RunHighScaleTest(5000000);   // 5 Million Elements
    RunHighScaleTest(10000000);  // 10 Million Elements
    return 0;
}
