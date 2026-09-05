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

// Include sorters
#include "vblock_sort.hpp"
#include "WikiSort/WikiSort.hpp"
#include "KotaSort/kota_clean.hpp"

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

// -------------------------------------------------------------
// Struct Definitions of Varying Sizes
// -------------------------------------------------------------

// 1. Small Struct (16 Bytes)
struct SmallStruct {
    int64_t key;
    uint32_t id;
    char payload[4];

    bool operator<(const SmallStruct& o) const { return key < o.key; }
};

// 2. Medium Struct (64 Bytes - Exactly 1 Cache Line)
struct MediumStruct {
    int64_t key;
    uint32_t id;
    char payload[52];

    bool operator<(const MediumStruct& o) const { return key < o.key; }
};

// 3. Large Struct (256 Bytes - 4 Cache Lines / DB Tuple)
struct LargeStruct {
    int64_t key;
    uint32_t id;
    char payload[244];

    bool operator<(const LargeStruct& o) const { return key < o.key; }
};

// 4. Huge Struct (1024 Bytes - 1 KB Document Record)
struct HugeStruct {
    int64_t key;
    uint32_t id;
    char payload[1012];

    bool operator<(const HugeStruct& o) const { return key < o.key; }
};

// 5. Giant Struct (2048 Bytes - 2 KB Heavyweight Payload)
struct GiantStruct {
    int64_t key;
    uint32_t id;
    char payload[2036];

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

template <typename T>
void run_benchmark_for_type(const std::string& type_name, size_t n, size_t trials = 3) {
    std::cout << "\n" << Color::BOLD << Color::CYAN
              << "========================================================================================\n"
              << "  STRUCT TYPE: " << Color::WHITE << type_name 
              << " (" << sizeof(T) << " Bytes per Element) | N = " << n << "\n"
              << "  Theoretical Fixed-512 Stack: " << (512 * sizeof(T)) / 1024.0 << " KB"
              << " | Byte-Budgeted 4KB Stack: " << (std::min<size_t>(512, 4096 / sizeof(T)) * sizeof(T)) / 1024.0 << " KB"
              << "\n"
              << "========================================================================================\n"
              << Color::RESET;

    std::cout << std::left  << std::setw(28) << "Algorithm"
              << std::right << std::setw(15) << "Stack Used"
              << std::right << std::setw(15) << "Heap Used"
              << std::right << std::setw(14) << "Time (ms)"
              << std::right << std::setw(14) << "Status"
              << "\n" << std::string(86, '-') << "\n";

    // Prepare random data with some duplicate keys to test stability
    std::mt19937 rng(1337);
    std::vector<T> master(n);
    for (size_t i = 0; i < n; ++i) {
        master[i].key = static_cast<int64_t>(rng() % (n / 2));
        master[i].id = static_cast<uint32_t>(i);
        std::memset(master[i].payload, static_cast<int>(i & 0xFF), sizeof(master[i].payload));
    }

    auto test_sorter = [&](const std::string& name, const std::string& stack_str, const std::string& heap_str, auto sort_func, bool expect_stable = true) {
        double total_ms = 0;
        bool ok = true;

        for (size_t t = 0; t < trials; ++t) {
            std::vector<T> arr = master;
            auto start = std::chrono::high_resolution_clock::now();
            sort_func(arr);
            auto end = std::chrono::high_resolution_clock::now();
            total_ms += std::chrono::duration<double, std::milli>(end - start).count();

            if (t == 0 && expect_stable) {
                ok = verify_stable(arr);
            }
        }

        double avg_ms = total_ms / trials;

        std::cout << std::left  << std::setw(28) << name
                  << std::right << std::setw(15) << stack_str
                  << std::right << std::setw(15) << heap_str
                  << std::right << std::setw(14) << std::fixed << std::setprecision(2) << avg_ms
                  << std::right << std::setw(14) << (expect_stable ? (ok ? (Color::GREEN + "PASS (STABLE)" + Color::RESET) : (Color::RED + "FAIL" + Color::RESET)) : (Color::YELLOW + "UNSTABLE*" + Color::RESET))
                  << "\n";
    };

    // 1. std::sort
    test_sorter("std::sort", "O(log N)", "0 B", [](std::vector<T>& a) {
        std::sort(a.begin(), a.end(), [](const T& x, const T& y) { return x.key < y.key; });
    }, false);

    // 2. std::stable_sort
    std::string std_heap = std::to_string((n * sizeof(T)) / (1024 * 1024)) + " MB";
    test_sorter("std::stable_sort", "O(log N)", std_heap, [](std::vector<T>& a) {
        std::stable_sort(a.begin(), a.end(), [](const T& x, const T& y) { return x.key < y.key; });
    });

    // 3. KotaSort (Pure In-Place)
    test_sorter("KotaSort (In-Place)", "O(1) ~1 KB", "0 B", [](std::vector<T>& a) {
        KotaClean::sort(a.data(), a.size(), [](const T& x, const T& y) { return x.key < y.key; });
    });

    // 4. WikiSort (Fixed 512 stack buffer)
    // Beware: On GiantStruct (2KB), WikiSort allocates 1MB on the stack!
    std::string wiki_stack = std::to_string((512 * sizeof(T)) / 1024) + " KB";
    test_sorter("WikiSort", wiki_stack, "0 B", [](std::vector<T>& a) {
        Wiki::Sort(a.begin(), a.end(), [](const T& x, const T& y) { return x.key < y.key; });
    });

    // 5. V-Block (Fixed 512: Legacy)
    std::string vblock_fixed_stack = std::to_string((512 * sizeof(T)) / 1024) + " KB";
    test_sorter("V-Block (Fixed 512)", vblock_fixed_stack, "0 B", [](std::vector<T>& a) {
        VBlock::SortFixed512(a.data(), a.size(), [](const T& x, const T& y) { return x.key < y.key; });
    });

    // 6. V-Block (Byte-Budgeted 4KB: Safe Default)
    size_t actual_stack_4k = (std::min<size_t>(512, 4096 / sizeof(T)) * sizeof(T));
    std::string vblock_budgeted_stack = std::to_string(actual_stack_4k / 1024) + " KB (" + std::to_string(std::min<size_t>(512, 4096 / sizeof(T))) + " elem)";
    test_sorter("V-Block (Byte-Budgeted 4KB)", vblock_budgeted_stack, "0 B", [](std::vector<T>& a) {
        VBlock::Sort<4096>(a.data(), a.size(), [](const T& x, const T& y) { return x.key < y.key; });
    });

    // 7. V-Block (Byte-Budgeted 16KB: L1/L2 High Performance)
    size_t actual_stack_16k = (std::min<size_t>(512, 16384 / sizeof(T)) * sizeof(T));
    std::string vblock_budgeted_16k_stack = std::to_string(actual_stack_16k / 1024) + " KB (" + std::to_string(std::min<size_t>(512, 16384 / sizeof(T))) + " elem)";
    test_sorter("V-Block (Byte-Budgeted 16KB)", vblock_budgeted_16k_stack, "0 B", [](std::vector<T>& a) {
        VBlock::Sort<16384>(a.data(), a.size(), [](const T& x, const T& y) { return x.key < y.key; });
    });
}

int main() {
    std::cout << Color::BOLD << Color::MAGENTA
              << "========================================================================\n"
              << "    V-BLOCKSORT STRUCT-SIZE & STACK FOOTPRINT BENCHMARK\n"
              << "========================================================================\n"
              << Color::RESET;

    const size_t N = 50000; // 50k elements to run comfortably across multi-kilobyte structs

    run_benchmark_for_type<SmallStruct>("SmallStruct (16B)", N);
    run_benchmark_for_type<MediumStruct>("MediumStruct (64B)", N);
    run_benchmark_for_type<LargeStruct>("LargeStruct (256B)", N);
    run_benchmark_for_type<HugeStruct>("HugeStruct (1024B / 1KB)", N);
    run_benchmark_for_type<GiantStruct>("GiantStruct (2048B / 2KB)", N);

    std::cout << "\n" << Color::BOLD << Color::GREEN
              << "All struct size benchmarks completed successfully!\n"
              << Color::RESET;

    return 0;
}
