#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cassert>

#include "vblock_sort.hpp"

namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
    const std::string GREEN   = "\033[32m";
    const std::string YELLOW  = "\033[33m";
    const std::string CYAN    = "\033[36m";
    const std::string MAGENTA = "\033[35m";
    const std::string RED     = "\033[31m";
}

// -------------------------------------------------------------
// Struct Definitions
// -------------------------------------------------------------
struct SmallStruct {
    int64_t key;
    uint32_t id;
    char payload[4]; // 16 Bytes
    bool operator<(const SmallStruct& o) const { return key < o.key; }
};

struct MediumStruct {
    int64_t key;
    uint32_t id;
    char payload[52]; // 64 Bytes (1 Cache Line)
    bool operator<(const MediumStruct& o) const { return key < o.key; }
};

struct LargeStruct {
    int64_t key;
    uint32_t id;
    char payload[244]; // 256 Bytes (4 Cache Lines)
    bool operator<(const LargeStruct& o) const { return key < o.key; }
};

struct HugeStruct {
    int64_t key;
    uint32_t id;
    char payload[1012]; // 1024 Bytes (1 KB)
    bool operator<(const HugeStruct& o) const { return key < o.key; }
};

struct GiantStruct {
    int64_t key;
    uint32_t id;
    char payload[2036]; // 2048 Bytes (2 KB)
    bool operator<(const GiantStruct& o) const { return key < o.key; }
};

template <typename T>
bool verify_stable(const std::vector<T>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) return false;
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) return false;
    }
    return true;
}

bool verify_sorted_int(const std::vector<int32_t>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i] < arr[i - 1]) return false;
    }
    return true;
}

// =========================================================================
// PART 1: PLAIN INT BENCHMARK (N = 100,000 & N = 1,000,000)
// =========================================================================
void run_int_benchmarks() {
    std::cout << Color::BOLD << Color::MAGENTA
              << "\n========================================================================================\n"
              << "     PART 1: PLAIN int32_t BENCHMARK: GNU __inplace_stable_sort vs V-BlockSort\n"
              << "========================================================================================\n"
              << Color::RESET;

    struct IntScenario {
        std::string name;
        size_t n;
        std::vector<int32_t> data;
    };

    std::mt19937 rng(42);
    std::vector<IntScenario> scenarios;

    // N = 100,000 scenarios
    const size_t N1 = 100000;
    {
        IntScenario sc{"Random Uniform (N=100k)", N1, {}};
        sc.data.resize(N1);
        for (size_t i = 0; i < N1; ++i) sc.data[i] = rng();
        scenarios.push_back(sc);
    }
    {
        IntScenario sc{"Binary / 2 Keys (N=100k)", N1, {}};
        sc.data.resize(N1);
        for (size_t i = 0; i < N1; ++i) sc.data[i] = rng() % 2;
        scenarios.push_back(sc);
    }
    {
        IntScenario sc{"Already Sorted (N=100k)", N1, {}};
        sc.data.resize(N1);
        for (size_t i = 0; i < N1; ++i) sc.data[i] = static_cast<int32_t>(i);
        scenarios.push_back(sc);
    }
    {
        IntScenario sc{"Strictly Reversed (N=100k)", N1, {}};
        sc.data.resize(N1);
        for (size_t i = 0; i < N1; ++i) sc.data[i] = static_cast<int32_t>(N1 - i);
        scenarios.push_back(sc);
    }
    {
        IntScenario sc{"Organ Pipe (Bitonic N=100k)", N1, {}};
        sc.data.resize(N1);
        for (size_t i = 0; i < N1 / 2; ++i) sc.data[i] = static_cast<int32_t>(i);
        for (size_t i = N1 / 2; i < N1; ++i) sc.data[i] = static_cast<int32_t>(N1 - i);
        scenarios.push_back(sc);
    }

    // N = 1,000,000 scenario (1 Million Ints Stress Test)
    const size_t N2 = 1000000;
    {
        IntScenario sc{"Random Uniform (N=1,000,000!)", N2, {}};
        sc.data.resize(N2);
        for (size_t i = 0; i < N2; ++i) sc.data[i] = rng();
        scenarios.push_back(sc);
    }

    std::cout << std::left  << std::setw(34) << "Scenario / Distribution"
              << std::right << std::setw(16) << "GNU In-Place (ms)"
              << std::right << std::setw(16) << "V-Block 4KB (ms)"
              << std::right << std::setw(16) << "Speedup"
              << std::right << std::setw(12) << "Status"
              << "\n" << std::string(94, '-') << "\n";

    for (const auto& sc : scenarios) {
        // Benchmark GNU std::__inplace_stable_sort
        std::vector<int32_t> arr_gnu = sc.data;
        auto t0 = std::chrono::high_resolution_clock::now();
        std::__inplace_stable_sort(arr_gnu.begin(), arr_gnu.end(), std::less<int32_t>());
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms_gnu = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Benchmark V-BlockSort (4KB Budget)
        std::vector<int32_t> arr_vb = sc.data;
        auto t2 = std::chrono::high_resolution_clock::now();
        VBlock::Sort<4096>(arr_vb.data(), arr_vb.size(), std::less<int32_t>());
        auto t3 = std::chrono::high_resolution_clock::now();
        double ms_vb = std::chrono::duration<double, std::milli>(t3 - t2).count();

        bool ok = verify_sorted_int(arr_vb);
        double speedup = (ms_vb > 0) ? (ms_gnu / ms_vb) : 0;

        std::cout << std::left  << std::setw(34) << sc.name
                  << std::right << std::setw(16) << std::fixed << std::setprecision(2) << ms_gnu
                  << std::right << std::setw(16) << std::fixed << std::setprecision(2) << ms_vb
                  << std::right << std::setw(15) << std::fixed << std::setprecision(1) << speedup << "x"
                  << std::right << std::setw(12) << (ok ? (Color::GREEN + "PASS" + Color::RESET) : (Color::RED + "FAIL" + Color::RESET))
                  << "\n";
    }
}

// =========================================================================
// PART 2: STRUCT SIZE COMPARISON (N = 50,000)
// =========================================================================
template <typename T>
void run_struct_test(const std::string& name, size_t n) {
    std::mt19937 rng(1337);
    std::vector<T> master(n);
    for (size_t i = 0; i < n; ++i) {
        master[i].key = static_cast<int64_t>(rng() % (n / 2));
        master[i].id = static_cast<uint32_t>(i);
        std::memset(master[i].payload, static_cast<int>(i & 0xFF), sizeof(master[i].payload));
    }

    // GNU In-Place
    std::vector<T> arr_gnu = master;
    auto t0 = std::chrono::high_resolution_clock::now();
    std::__inplace_stable_sort(arr_gnu.begin(), arr_gnu.end(), [](const T& a, const T& b) { return a.key < b.key; });
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms_gnu = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // V-Block 4KB
    std::vector<T> arr_vb = master;
    auto t2 = std::chrono::high_resolution_clock::now();
    VBlock::Sort<4096>(arr_vb.data(), arr_vb.size(), [](const T& a, const T& b) { return a.key < b.key; });
    auto t3 = std::chrono::high_resolution_clock::now();
    double ms_vb = std::chrono::duration<double, std::milli>(t3 - t2).count();
    bool ok_vb = verify_stable(arr_vb);

    double speedup = (ms_vb > 0) ? (ms_gnu / ms_vb) : 0;

    std::cout << std::left  << std::setw(30) << name
              << std::right << std::setw(12) << sizeof(T)
              << std::right << std::setw(18) << std::fixed << std::setprecision(2) << ms_gnu
              << std::right << std::setw(18) << std::fixed << std::setprecision(2) << ms_vb
              << std::right << std::setw(14) << std::fixed << std::setprecision(1) << speedup << "x"
              << std::right << std::setw(14) << (ok_vb ? (Color::GREEN + "STABLE" + Color::RESET) : (Color::RED + "FAIL" + Color::RESET))
              << "\n";
}

void run_struct_benchmarks() {
    std::cout << Color::BOLD << Color::CYAN
              << "\n========================================================================================\n"
              << "     PART 2: STRUCT SIZES BENCHMARK: GNU __inplace_stable_sort vs V-BlockSort\n"
              << "========================================================================================\n"
              << Color::RESET;

    const size_t N = 50000;

    std::cout << std::left  << std::setw(30) << "Struct Type"
              << std::right << std::setw(12) << "Bytes/Elem"
              << std::right << std::setw(18) << "GNU In-Place (ms)"
              << std::right << std::setw(18) << "V-Block 4KB (ms)"
              << std::right << std::setw(14) << "Speedup"
              << std::right << std::setw(14) << "Stability"
              << "\n" << std::string(106, '-') << "\n";

    run_struct_test<SmallStruct>("SmallStruct (16B)", N);
    run_struct_test<MediumStruct>("MediumStruct (64B)", N);
    run_struct_test<LargeStruct>("LargeStruct (256B)", N);
    run_struct_test<HugeStruct>("HugeStruct (1024B / 1KB)", N);
    run_struct_test<GiantStruct>("GiantStruct (2048B / 2KB)", N);
}

int main() {
    run_int_benchmarks();
    run_struct_benchmarks();
    std::cout << "\n" << Color::BOLD << Color::GREEN << "GNU In-Place vs V-Block comparison complete!\n" << Color::RESET;
    return 0;
}
