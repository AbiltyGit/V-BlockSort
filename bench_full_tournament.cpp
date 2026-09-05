#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>

// External sort headers
#include "vblock_sort.hpp"

struct Element {
    int64_t key;
    uint32_t id;

    bool operator<(const Element& other) const {
        return key < other.key;
    }
    bool operator<=(const Element& other) const {
        return key <= other.key;
    }
    bool operator>(const Element& other) const {
        return key > other.key;
    }
    bool operator==(const Element& other) const {
        return key == other.key;
    }
};

inline bool element_less(const Element& a, const Element& b) {
    return a.key < b.key;
}

// 1. GrailSort Wrapper
namespace GrailWrapper {
    inline int grail_compare(const Element* a, const Element* b) {
        if (a->key < b->key) return -1;
        if (a->key > b->key) return 1;
        return 0;
    }
}

#define SORT_TYPE Element
#define SORT_CMP GrailWrapper::grail_compare
#include "GrailSort/GrailSort.h"
#undef SORT_TYPE
#undef SORT_CMP

#include "WikiSort/WikiSort.hpp"
#include "KotaSort/kota_clean.hpp"

enum class Distribution {
    RANDOM_UNIFORM,
    BINARY_2_KEYS,
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

std::string get_dist_name(Distribution d) {
    switch (d) {
        case Distribution::RANDOM_UNIFORM:      return "Random Uniform";
        case Distribution::BINARY_2_KEYS:       return "Binary (2 Keys)";
        case Distribution::FOUR_KEYS:           return "4 Keys";
        case Distribution::SQRT_KEYS:           return "Sqrt(N) Keys";
        case Distribution::ALL_EQUAL:           return "All Equal";
        case Distribution::SORTED:              return "Already Sorted";
        case Distribution::REVERSED:            return "Strictly Reversed";
        case Distribution::ALMOST_SORTED:       return "Almost Sorted (95%)";
        case Distribution::ORGAN_PIPE:          return "Organ Pipe";
        case Distribution::REVERSE_ORGAN_PIPE:  return "Reverse Organ Pipe";
        case Distribution::SAWTOOTH:            return "Sawtooth (16 Ramps)";
        case Distribution::ROTATED:             return "Rotated (Shift 1)";
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
        case Distribution::ALMOST_SORTED: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            size_t swaps = n / 20;
            for (size_t i = 0; i < swaps; ++i) {
                size_t idx1 = rng() % n;
                size_t idx2 = rng() % n;
                std::swap(arr[idx1], arr[idx2]);
            }
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
        case Distribution::ROTATED: {
            for (size_t i = 0; i < n; ++i)
                arr[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            if (n > 0) {
                Element first = arr[0];
                for (size_t i = 1; i < n; ++i) arr[i - 1] = arr[i];
                arr[n - 1] = first;
            }
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
    std::string category;
    std::function<void(Element*, size_t)> sort_fn;
};

int main(int argc, char** argv) {
    size_t N = 100000;
    int iters = 3;
    if (argc > 1) N = std::stoull(argv[1]);
    if (argc > 2) iters = std::stoi(argv[2]);

    std::vector<Sorter> sorters = {
        {"std::sort", "Unstable", [](Element* arr, size_t n) {
            std::sort(arr, arr + n, element_less);
        }},
        {"std::stable_sort", "O(N) heap", [](Element* arr, size_t n) {
            std::stable_sort(arr, arr + n, element_less);
        }},
        {"std::__inplace_stable", "In-Place", [](Element* arr, size_t n) {
            std::__inplace_stable_sort(arr, arr + n, element_less);
        }},
        {"GrailSort", "In-Place", [](Element* arr, size_t n) {
            GrailSort(arr, static_cast<int>(n));
        }},
        {"WikiSort", "512-buf", [](Element* arr, size_t n) {
            Wiki::Sort(arr, arr + n, element_less);
        }},
        {"KotaSort", "In-Place", [](Element* arr, size_t n) {
            KotaClean::sort(arr, n, element_less);
        }},
        {"VB (0 B SAT)", "In-Place 0B", [](Element* arr, size_t n) {
            VBlock::Sort<0>(arr, n, element_less);
        }},
        {"VB (1 KB)", "1KB Stack", [](Element* arr, size_t n) {
            VBlock::Sort<1024>(arr, n, element_less);
        }},
        {"VB (2 KB)", "2KB Stack", [](Element* arr, size_t n) {
            VBlock::Sort<2048>(arr, n, element_less);
        }},
        {"VB (4 KB)", "4KB Stack", [](Element* arr, size_t n) {
            VBlock::Sort<4096>(arr, n, element_less);
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
        Distribution::ALMOST_SORTED,
        Distribution::ORGAN_PIPE,
        Distribution::REVERSE_ORGAN_PIPE,
        Distribution::SAWTOOTH,
        Distribution::ROTATED
    };

    std::cout << "\n========================================================================================================================\n";
    std::cout << "  V-BLOCKSORT TOURNAMENT BENCHMARK (N = " << N << ", " << iters << " runs avg)\n";
    std::cout << "========================================================================================================================\n";

    // Print Header
    std::cout << std::left << std::setw(22) << "Distribution"
              << std::right << std::setw(11) << "std::sort"
              << std::setw(12) << "std::stable"
              << std::setw(12) << "GNU in-pl"
              << std::setw(11) << "GrailSort"
              << std::setw(11) << "WikiSort"
              << std::setw(11) << "KotaSort"
              << std::setw(12) << "VB (0 B SAT)"
              << std::setw(10) << "VB (1 KB)"
              << std::setw(10) << "VB (2 KB)"
              << std::setw(10) << "VB (4 KB)"
              << "\n";
    std::cout << std::string(132, '-') << "\n" << std::flush;

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
            if (s.name != "std::sort") {
                if (!verify_sorted_and_stable(work)) {
                    std::cerr << "\n[STABILITY ERROR in " << s.name << " on " << dist_name << "]\n";
                }
            }

            double time_ms = measure_ms([&]() {
                work = master;
                s.sort_fn(work.data(), N);
            }, iters);

            int width = (si == 0) ? 9 : ((si == 1 || si == 2) ? 10 : ((si == 6) ? 10 : 8));
            std::cout << std::setw(width) << time_ms << "ms" << std::flush;
        }
        std::cout << "\n" << std::flush;
    }
    std::cout << std::string(132, '=') << "\n\n" << std::flush;

    return 0;
}
