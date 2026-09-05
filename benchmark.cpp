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

// Global comparison counter
static uint64_t g_comparisons = 0;

struct Element {
    int64_t key;
    uint32_t id;

    bool operator<(const Element& other) const {
        g_comparisons++;
        return key < other.key;
    }
    bool operator<=(const Element& other) const {
        g_comparisons++;
        return key <= other.key;
    }
    bool operator>(const Element& other) const {
        g_comparisons++;
        return key > other.key;
    }
    bool operator==(const Element& other) const {
        g_comparisons++;
        return key == other.key;
    }
};

inline bool element_less(const Element& a, const Element& b) {
    g_comparisons++;
    return a.key < b.key;
}

// 1. GrailSort Wrapper
namespace GrailWrapper {
    inline int grail_compare(const Element* a, const Element* b) {
        g_comparisons++;
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

// 2. WikiSort Header
#include "WikiSort/WikiSort.hpp"

// 3. KotaSort Clean C++ Port
#include "KotaSort/kota_clean.hpp"

// 6. V-BlockSort (Final: Layers 0+1+2+3)
#include "vblock_sort.hpp"

// 7. V-BlockSort SAT-Hybrid
#include "vblock_sort_sat_hybrid.hpp"

// Terminal Colors
namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
    const std::string DIM     = "\033[2m";
    const std::string RED     = "\033[31m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string BLUE    = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN    = "\033[36m";
    const std::string WHITE   = "\033[37m";
}

struct SortResult {
    std::string algorithm_name;
    std::string mem_type;
    double time_ms;
    uint64_t comparisons;
    bool is_sorted;
    bool is_stable;
    bool expected_stable;
};

// Data Distribution Generator
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

std::string get_distribution_name(Distribution d) {
    switch (d) {
        case Distribution::RANDOM_UNIFORM:      return "Random Uniform (Average Case)";
        case Distribution::BINARY_2_KEYS:       return "Binary / 2 Keys (Extreme Low Keys)";
        case Distribution::FOUR_KEYS:           return "4 Keys (Low Key Diversity)";
        case Distribution::SQRT_KEYS:           return "Sqrt(N) Keys (Borderline Buffer)";
        case Distribution::ALL_EQUAL:           return "All Equal (1 Key)";
        case Distribution::SORTED:              return "Already Sorted (Best Case)";
        case Distribution::REVERSED:            return "Strictly Reversed (Worst Inversions)";
        case Distribution::ALMOST_SORTED:       return "Almost Sorted (95% In-Order)";
        case Distribution::ORGAN_PIPE:          return "Organ Pipe (Ascending then Descending)";
        case Distribution::REVERSE_ORGAN_PIPE:  return "Reverse Organ Pipe (V-Shape)";
        case Distribution::SAWTOOTH:            return "Sawtooth (16 Periodic Ramps)";
        case Distribution::ROTATED:             return "Rotated Sorted (Cyclic Shift)";
    }
    return "Unknown";
}

std::vector<Element> generate_data(size_t n, Distribution dist, uint64_t seed = 42) {
    std::vector<Element> arr(n);
    std::mt19937_64 rng(seed);

    switch (dist) {
        case Distribution::RANDOM_UNIFORM: {
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(rng() & 0x7FFFFFFF), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::BINARY_2_KEYS: {
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(rng() % 2), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::FOUR_KEYS: {
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(rng() % 4), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::SQRT_KEYS: {
            int64_t k = std::max(2L, static_cast<int64_t>(std::sqrt(n)));
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(rng() % k), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::ALL_EQUAL: {
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {42, static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::SORTED: {
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::REVERSED: {
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(n - i), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::ALMOST_SORTED: {
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            }
            // Swap 5% of pairs randomly
            size_t swaps = n / 20;
            for (size_t s = 0; s < swaps; ++s) {
                size_t idx1 = rng() % n;
                size_t idx2 = rng() % n;
                std::swap(arr[idx1], arr[idx2]);
            }
            break;
        }
        case Distribution::ORGAN_PIPE: {
            size_t mid = n / 2;
            for (size_t i = 0; i < mid; ++i) {
                arr[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};
            }
            for (size_t i = mid; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(n - i), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::REVERSE_ORGAN_PIPE: {
            size_t mid = n / 2;
            for (size_t i = 0; i < mid; ++i) {
                arr[i] = {static_cast<int64_t>(mid - i), static_cast<uint32_t>(i)};
            }
            for (size_t i = mid; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(i - mid), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::SAWTOOTH: {
            int runs = 16;
            size_t run_len = (n + runs - 1) / runs;
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>(i % run_len), static_cast<uint32_t>(i)};
            }
            break;
        }
        case Distribution::ROTATED: {
            size_t shift = n / 2;
            for (size_t i = 0; i < n; ++i) {
                arr[i] = {static_cast<int64_t>((i + shift) % n), static_cast<uint32_t>(i)};
            }
            break;
        }
    }
    return arr;
}

// Verification function
void verify_result(const std::vector<Element>& arr, bool& is_sorted, bool& is_stable) {
    is_sorted = true;
    is_stable = true;
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) {
            is_sorted = false;
        }
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) {
            is_stable = false;
        }
    }
}

// Benchmark Runner
struct SorterConfig {
    std::string name;
    std::string mem_type;
    bool expected_stable;
    std::function<void(Element*, size_t)> func;
};

std::vector<SorterConfig> get_all_sorters() {
    return {
        {
            "std::sort",
            "O(log N) stack",
            false,
            [](Element* arr, size_t n) {
                std::sort(arr, arr + n, element_less);
            }
        },
        {
            "std::stable_sort",
            "O(N) buffer",
            true,
            [](Element* arr, size_t n) {
                std::stable_sort(arr, arr + n, element_less);
            }
        },
        {
            "std::__inplace_stable",
            "O(1) in-place",
            true,
            [](Element* arr, size_t n) {
                std::__inplace_stable_sort(arr, arr + n, element_less);
            }
        },
        {
            "GrailSort (In-Place)",
            "O(1) in-place",
            true,
            [](Element* arr, size_t n) {
                GrailSort(arr, static_cast<int>(n));
            }
        },
        {
            "GrailSort (512-Buf)",
            "O(1) 512-buf",
            true,
            [](Element* arr, size_t n) {
                GrailSortWithBuffer(arr, static_cast<int>(n));
            }
        },
        {
            "WikiSort",
            "O(1) 512-buf",
            true,
            [](Element* arr, size_t n) {
                Wiki::Sort(arr, arr + n, element_less);
            }
        },
        {
            "KotaSort (In-Place)",
            "O(1) in-place",
            true,
            [](Element* arr, size_t n) {
                KotaClean::sort(arr, n, element_less);
            }
        },
        {
            "V-BlockSort (Final)",
            "O(1) in-place",
            true,
            [](Element* arr, size_t n) {
                VBlock::Sort(arr, n, element_less);
            }
        },
        {
            "V-Block (SAT-Hybrid)",
            "O(1) in-place",
            true,
            [](Element* arr, size_t n) {
                VBlockSat::Sort(arr, n, element_less);
            }
        }
    };
}

void print_header(size_t n, const std::string& dist_name) {
    std::cout << "\n" << Color::BOLD << Color::CYAN 
              << "========================================================================================\n"
              << "  DISTRIBUTION: " << Color::WHITE << dist_name << Color::CYAN 
              << " | N = " << Color::YELLOW << n << Color::CYAN << "\n"
              << "========================================================================================" 
              << Color::RESET << "\n";
    
    std::cout << Color::BOLD
              << std::left  << std::setw(25) << "Algorithm"
              << std::left  << std::setw(16) << "Memory Type"
              << std::right << std::setw(12) << "Time (ms)"
              << std::right << std::setw(16) << "Comparisons"
              << std::right << std::setw(11) << "Correct?"
              << std::right << std::setw(15) << "Stability"
              << Color::RESET << "\n";
    
    std::cout << std::string(95, '-') << "\n";
}

void print_result_row(const SortResult& res) {
    std::cout << std::left  << std::setw(25) << res.algorithm_name
              << std::left  << std::setw(16) << (Color::DIM + res.mem_type + Color::RESET)
              << std::right << std::setw(12) << std::fixed << std::setprecision(2) << res.time_ms
              << std::right << std::setw(16) << res.comparisons;

    // Correctness Badge
    std::cout << "  ";
    if (res.is_sorted) {
        std::cout << Color::GREEN << std::setw(9) << "PASS" << Color::RESET;
    } else {
        std::cout << Color::RED << Color::BOLD << std::setw(9) << "FAIL" << Color::RESET;
    }

    // Stability Badge
    std::cout << "  ";
    if (res.is_stable) {
        std::cout << Color::GREEN << std::setw(13) << "STABLE" << Color::RESET;
    } else if (!res.expected_stable) {
        std::cout << Color::YELLOW << std::setw(13) << "UNSTABLE*" << Color::RESET;
    } else {
        std::cout << Color::RED << Color::BOLD << std::setw(13) << "UNSTABLE!" << Color::RESET;
    }

    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    std::vector<size_t> test_sizes = {100000};
    if (argc > 1) {
        test_sizes.clear();
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            try {
                size_t sz = std::stoull(arg);
                if (sz > 0) test_sizes.push_back(sz);
            } catch (...) {}
        }
        if (test_sizes.empty()) test_sizes = {100000};
    }

    std::vector<Distribution> test_distributions = {
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

    auto sorters = get_all_sorters();

    std::cout << Color::BOLD << Color::MAGENTA 
              << "\n========================================================================\n"
              << "       BLOCK SORT & STABLE MERGE SORT BENCHMARK & COMPARISON SUITE\n"
              << "========================================================================\n"
              << Color::RESET
              << "Algorithms Evaluated:\n"
              << "  - std::sort               : Fast C++ Introsort (Unstable baseline)\n"
              << "  - std::stable_sort        : Standard Stable Mergesort with O(N) buffer\n"
              << "  - std::__inplace_stable   : libstdc++ In-Place Stable Sort (O(N log^2 N))\n"
              << "  - GrailSort (In-Place)    : Andrey Astrelin's Pure In-Place Block Sort (O(1))\n"
              << "  - GrailSort (512-Buf)     : GrailSort with 512-Element Static Buffer\n"
              << "  - WikiSort                : Mike McFadden's Block Sort (O(1) with cache)\n"
              << "  - KotaSort (In-Place)     : aphitorite's KotaSort (Full In-Place C++ Port)\n"
              << "  - Wall-L (L=3, L=6)       : Layered Mergesort (O(N) Dynamic Memory Allocation)\n"
              << "\n";

    for (size_t n : test_sizes) {
        for (auto dist : test_distributions) {
            std::string dist_name = get_distribution_name(dist);
            print_header(n, dist_name);

            // Generate master dataset
            std::vector<Element> master = generate_data(n, dist);

            for (const auto& sorter : sorters) {
                // Prepare copy
                std::vector<Element> arr = master;
                g_comparisons = 0;

                auto start_time = std::chrono::high_resolution_clock::now();
                sorter.func(arr.data(), arr.size());
                auto end_time = std::chrono::high_resolution_clock::now();

                double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

                bool is_sorted = false;
                bool is_stable = false;
                verify_result(arr, is_sorted, is_stable);

                SortResult res {
                    sorter.name,
                    sorter.mem_type,
                    duration_ms,
                    g_comparisons,
                    is_sorted,
                    is_stable,
                    sorter.expected_stable
                };

                print_result_row(res);
            }
        }
    }

    std::cout << "\n" << Color::DIM << "* UNSTABLE*: std::sort is intentionally not stable by C++ specification.\n" 
              << Color::RESET << "\n";

    return 0;
}
