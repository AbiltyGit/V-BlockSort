#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <cassert>

// Include Virtual Key Sort
#include "vblock_sort_virtual_key.hpp"

// Include Main V-BlockSort
#include "../../vblock_sort.hpp"

// External sorts
struct Element {
    int64_t key;
    uint32_t id;

    bool operator<(const Element& o) const { return key < o.key; }
    bool operator<=(const Element& o) const { return key <= o.key; }
    bool operator>(const Element& o) const { return key > o.key; }
    bool operator==(const Element& o) const { return key == o.key; }
};

inline bool elem_less(const Element& a, const Element& b) {
    return a.key < b.key;
}

namespace GrailWrapper {
    inline int grail_compare(const Element* a, const Element* b) {
        if (a->key < b->key) return -1;
        if (a->key > b->key) return 1;
        return 0;
    }
}

#define SORT_TYPE Element
#define SORT_CMP GrailWrapper::grail_compare
#include "../../GrailSort/GrailSort.h"
#undef SORT_TYPE
#undef SORT_CMP

#include "../../WikiSort/WikiSort.hpp"
#include "../../KotaSort/kota_clean.hpp"

enum class Distribution {
    RANDOM_UNIFORM,
    BINARY_2_KEYS,
    FOUR_KEYS,
    SQRT_KEYS,
    ALL_EQUAL,
    SORTED,
    REVERSED,
    ORGAN_PIPE,
    REVERSE_ORGAN_PIPE,
    SAWTOOTH
};

std::string get_dist_name(Distribution d) {
    switch (d) {
        case Distribution::RANDOM_UNIFORM:      return "Random Uniform";
        case Distribution::BINARY_2_KEYS:       return "Binary (2 Keys)";
        case Distribution::FOUR_KEYS:           return "4 Keys";
        case Distribution::SQRT_KEYS:           return "Sqrt(N) Keys";
        case Distribution::ALL_EQUAL:           return "All Equal";
        case Distribution::SORTED:              return "Already Sorted";
        case Distribution::REVERSED:            return "Strictly Reversed";
        case Distribution::ORGAN_PIPE:          return "Organ Pipe";
        case Distribution::REVERSE_ORGAN_PIPE:  return "Reverse Organ Pipe";
        case Distribution::SAWTOOTH:            return "Sawtooth (16 Ramps)";
    }
    return "Unknown";
}

std::vector<Element> generate_data(size_t n, Distribution dist, uint64_t seed = 42) {
    std::vector<Element> arr(n);
    std::mt19937_64 rng(seed);

    switch (dist) {
        case Distribution::RANDOM_UNIFORM: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(rng() & 0x7FFFFFFF), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::BINARY_2_KEYS: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(rng() % 2), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::FOUR_KEYS: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(rng() % 4), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::SQRT_KEYS: {
            int64_t k = std::max<int64_t>(1, static_cast<int64_t>(std::sqrt(n)));
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(rng() % k), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::ALL_EQUAL: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {42, static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::SORTED: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::REVERSED: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(n - i), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::ORGAN_PIPE: {
            size_t mid = n / 2;
            for (size_t i = 0; i < mid; ++i)
                arr[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            for (size_t i = mid; i < n; ++i)
                arr[i] = {static_cast<int64_t>(n - 1 - i), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::REVERSE_ORGAN_PIPE: {
            size_t mid = n / 2;
            for (size_t i = 0; i < mid; ++i)
                arr[i] = {static_cast<int64_t>(mid - i), static_cast<uint32_t>(i)};
            for (size_t i = mid; i < n; ++i)
                arr[i] = {static_cast<int64_t>(i - mid), static_cast<uint32_t>(i)};
            break;
        }
        case Distribution::SAWTOOTH: {
            size_t num_ramps = 16;
            size_t ramp_len = (n + num_ramps - 1) / num_ramps;
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(i % ramp_len), static_cast<uint32_t>(i)};
            break;
        }
    }
    return arr;
}

bool verify_sorted_and_stable(const std::vector<Element>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) return false;
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) return false;
    }
    return true;
}

template<typename Func>
double measure_ms(Func&& fn, int iters = 3) {
    fn(); // Warmup
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        fn();
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() / iters;
}

struct Sorter {
    std::string name;
    std::function<void(Element*, size_t)> sort_fn;
};

int main(int argc, char** argv) {
    size_t N = 50000;
    int iters = 3;
    if (argc > 1) N = std::stoull(argv[1]);
    if (argc > 2) iters = std::stoi(argv[2]);

    std::vector<Sorter> sorters = {
        {"std::stable_sort", [](Element* arr, size_t n) {
            std::stable_sort(arr, arr + n, elem_less);
        }},
        {"GrailSort", [](Element* arr, size_t n) {
            GrailSort(arr, static_cast<int>(n));
        }},
        {"WikiSort", [](Element* arr, size_t n) {
            Wiki::Sort(arr, arr + n, elem_less);
        }},
        {"KotaSort", [](Element* arr, size_t n) {
            KotaClean::sort(arr, n, elem_less);
        }},
        {"V-Block (4KB Stack)", [](Element* arr, size_t n) {
            VBlock::Sort<4096>(arr, n, elem_less);
        }},
        {"V-Block (0B SAT)", [](Element* arr, size_t n) {
            VBlock::Sort<0>(arr, n, elem_less);
        }},
        {"V-Block (Virtual-Key)", [](Element* arr, size_t n) {
            VBlockVK::Sort(arr, arr + n, elem_less);
        }}
    };

    std::vector<Distribution> dists = {
        Distribution::RANDOM_UNIFORM,
        Distribution::BINARY_2_KEYS,
        Distribution::FOUR_KEYS,
        Distribution::SQRT_KEYS,
        Distribution::ALL_EQUAL,
        Distribution::SORTED,
        Distribution::REVERSED,
        Distribution::ORGAN_PIPE,
        Distribution::REVERSE_ORGAN_PIPE,
        Distribution::SAWTOOTH
    };

    std::cout << "\n===============================================================================================================\n";
    std::cout << "  VIRTUAL-KEY BLOCK SORT BENCHMARK (N = " << N << ", " << iters << " runs avg)\n";
    std::cout << "  Testing: Virtual-Key (Adjacent Tag Inversion) vs SymMerge V-Block vs Classic Block Sorts\n";
    std::cout << "===============================================================================================================\n";

    std::cout << std::left << std::setw(22) << "Distribution"
              << std::right << std::setw(14) << "std::stable"
              << std::setw(12) << "GrailSort"
              << std::setw(12) << "WikiSort"
              << std::setw(12) << "KotaSort"
              << std::setw(14) << "VB (4KB Stack)"
              << std::setw(14) << "VB (0B SAT)"
              << std::setw(16) << "VB (Virtual-Key)"
              << "\n";
    std::cout << std::string(112, '-') << "\n" << std::flush;

    for (auto d : dists) {
        std::vector<Element> master = generate_data(N, d);
        std::vector<Element> work = master;
        std::string dist_name = get_dist_name(d);

        std::cout << std::left << std::setw(22) << dist_name << std::right << std::fixed << std::setprecision(2) << std::flush;

        for (size_t si = 0; si < sorters.size(); ++si) {
            const auto& s = sorters[si];
            // Verify stability on one pass
            work = master;
            s.sort_fn(work.data(), N);
            if (!verify_sorted_and_stable(work)) {
                std::cerr << "\n[STABILITY ERROR in " << s.name << " on " << dist_name << "]\n";
            }

            double time_ms = measure_ms([&]() {
                work = master;
                s.sort_fn(work.data(), N);
            }, iters);

            int width = (si == 0) ? 12 : ((si == 4 || si == 5) ? 12 : ((si == 6) ? 14 : 10));
            std::cout << std::setw(width) << time_ms << "ms" << std::flush;
        }
        std::cout << "\n" << std::flush;
    }
    std::cout << std::string(112, '=') << "\n\n" << std::flush;

    return 0;
}
