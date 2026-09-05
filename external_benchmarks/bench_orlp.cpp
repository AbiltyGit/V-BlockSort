#include <random>
#include <ctime>
#include <vector>
#include <iostream>
#include <chrono>
#include <utility>
#include <array>
#include <type_traits>
#include <functional>
#include <string>
#include <iomanip>
#include <cstdint>
#include <algorithm>

#include "pdqsort/pdqsort.h"
#include "../include/vblock/vblock_sort.hpp"

// Terminal Colors
namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string CYAN    = "\033[36m";
    const std::string MAGENTA = "\033[35m";
}

#ifdef _WIN32
    #include <intrin.h>
    #define rdtsc __rdtsc
#else
    #if defined(__x86_64__) || defined(_M_X64)
        static __inline__ unsigned long long rdtsc() {
            unsigned hi, lo;
            __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
            return ((unsigned long long) lo) | (((unsigned long long) hi) << 32);
        }
    #elif defined(__aarch64__)
        static __inline__ unsigned long long rdtsc() {
            uint64_t val;
            asm volatile("mrs %0, cntvct_el0" : "=r"(val));
            return val;
        }
    #else
        static __inline__ unsigned long long rdtsc() {
            return std::chrono::high_resolution_clock::now().time_since_epoch().count();
        }
    #endif
#endif

// =========================================================================
// Orson Peters (pdqsort) Standard Distributions
// =========================================================================

std::vector<int> shuffled_int(int size, std::mt19937_64& rng) {
    std::vector<int> v; v.reserve(size);
    for (int i = 0; i < size; ++i) v.push_back(i);
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

std::vector<int> shuffled_16_values_int(int size, std::mt19937_64& rng) {
    std::vector<int> v; v.reserve(size);
    for (int i = 0; i < size; ++i) v.push_back(i % 16);
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

std::vector<int> all_equal_int(int size, std::mt19937_64&) {
    std::vector<int> v; v.reserve(size);
    for (int i = 0; i < size; ++i) v.push_back(0);
    return v;
}

std::vector<int> ascending_int(int size, std::mt19937_64&) {
    std::vector<int> v; v.reserve(size);
    for (int i = 0; i < size; ++i) v.push_back(i);
    return v;
}

std::vector<int> descending_int(int size, std::mt19937_64&) {
    std::vector<int> v; v.reserve(size);
    for (int i = size - 1; i >= 0; --i) v.push_back(i);
    return v;
}

std::vector<int> pipe_organ_int(int size, std::mt19937_64&) {
    std::vector<int> v; v.reserve(size);
    for (int i = 0; i < size/2; ++i) v.push_back(i);
    for (int i = size/2; i < size; ++i) v.push_back(size - i);
    return v;
}

std::vector<int> push_front_int(int size, std::mt19937_64&) {
    std::vector<int> v; v.reserve(size);
    for (int i = 1; i < size; ++i) v.push_back(i);
    v.push_back(0);
    return v;
}

std::vector<int> push_middle_int(int size, std::mt19937_64&) {
    std::vector<int> v; v.reserve(size);
    for (int i = 0; i < size; ++i) {
        if (i != size/2) v.push_back(i);
    }
    v.push_back(size/2);
    return v;
}

int main() {
    std::cout << Color::BOLD << Color::MAGENTA
              << "========================================================================================\n"
              << "   ORSON PETERS (PDQSORT) BENCHMARK HARNESS: INDUSTRY-STANDARD COMPARISON\n"
              << "========================================================================================\n"
              << Color::RESET;

    auto seed = std::time(0);
    std::mt19937_64 el(seed);

    typedef std::vector<int> (*DistrF)(int, std::mt19937_64&);
    typedef void (*SortF)(std::vector<int>::iterator, std::vector<int>::iterator);

    std::pair<std::string, DistrF> distributions[] = {
        {"shuffled_int",            shuffled_int},
        {"shuffled_16_values_int",   shuffled_16_values_int},
        {"all_equal_int",           all_equal_int},
        {"ascending_int",           ascending_int},
        {"descending_int",          descending_int},
        {"pipe_organ_int",          pipe_organ_int},
        {"push_front_int",          push_front_int},
        {"push_middle_int",         push_middle_int}
    };

    std::pair<std::string, SortF> sorts[] = {
        {"pdqsort", [](auto b, auto e) { pdqsort(b, e, std::less<int>()); }},
        {"std::sort", [](auto b, auto e) { std::sort(b, e, std::less<int>()); }},
        {"std::stable_sort", [](auto b, auto e) { std::stable_sort(b, e, std::less<int>()); }},
        {"GNU __inplace_stable", [](auto b, auto e) { std::__inplace_stable_sort(b, e, std::less<int>()); }},
        {"V-BlockSort (4KB)", [](auto b, auto e) { VBlock::Sort<4096>(b, e, std::less<int>()); }}
    };

    const int test_sizes[] = {100000, 1000000};

    for (int size : test_sizes) {
        std::cout << "\n" << Color::BOLD << Color::CYAN
                  << ">>> ARRAY SIZE N = " << size << (size == 1000000 ? " (1 MILLION INTEGERS)" : "")
                  << "\n" << std::string(88, '=') << Color::RESET << "\n";

        std::cout << std::left  << std::setw(26) << "Distribution"
                  << std::right << std::setw(12) << "pdqsort"
                  << std::right << std::setw(12) << "std::sort"
                  << std::right << std::setw(14) << "std::stable"
                  << std::right << std::setw(16) << "GNU in-place"
                  << std::right << std::setw(16) << "V-Block (4KB)"
                  << "\n" << std::string(96, '-') << "\n";

        for (auto& dist : distributions) {
            std::cout << std::left << std::setw(26) << dist.first;

            for (auto& sort : sorts) {
                // Generate base array with reproducible seed per distribution
                el.seed(seed + 1337);
                std::vector<int> master = dist.second(size, el);

                // Warmup
                std::vector<int> warmup = master;
                sort.second(warmup.begin(), warmup.end());

                // Benchmark iterations (at least 5 trials or 500ms)
                size_t trials = (size >= 1000000) ? 3 : 7;
                std::vector<double> ms_runs;
                ms_runs.reserve(trials);

                for (size_t t = 0; t < trials; ++t) {
                    std::vector<int> arr = master;
                    auto t0 = std::chrono::high_resolution_clock::now();
                    sort.second(arr.begin(), arr.end());
                    auto t1 = std::chrono::high_resolution_clock::now();
                    ms_runs.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                }

                std::sort(ms_runs.begin(), ms_runs.end());
                double median_ms = ms_runs[ms_runs.size() / 2];

                if (sort.first == "V-Block (4KB)") {
                    std::cout << std::right << std::setw(16) << std::fixed << std::setprecision(2)
                              << (median_ms < 10.0 ? Color::GREEN : Color::BOLD) << median_ms << " ms" << Color::RESET;
                } else if (sort.first == "GNU __inplace_stable") {
                    std::cout << std::right << std::setw(16) << std::fixed << std::setprecision(2) << median_ms << " ms";
                } else if (sort.first == "std::stable_sort") {
                    std::cout << std::right << std::setw(14) << std::fixed << std::setprecision(2) << median_ms << " ms";
                } else {
                    std::cout << std::right << std::setw(12) << std::fixed << std::setprecision(2) << median_ms << " ms";
                }
            }
            std::cout << "\n";
        }
    }

    std::cout << "\n" << Color::BOLD << Color::GREEN << "Orson Peters pdqsort benchmark suite completed!\n" << Color::RESET;
    return 0;
}
