#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include "vblock/vblock_sort.hpp"

// Access GNU internal in-place stable sort
namespace __gnu_test {
    template<typename RandomAccessIterator, typename Compare>
    void gnu_inplace_stable_sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
        std::__inplace_stable_sort(first, last, comp);
    }
}

struct Item {
    int key;
    int id;
    bool operator<(const Item& o) const { return key < o.key; }
};

struct BenchResult {
    double std_sort_ms;
    double std_stable_ms;
    double gnu_inplace_ms;
    double vblock_0B_ms;
    double vblock_1KB_ms;
    double vblock_2KB_ms;
    double vblock_4KB_ms;
};

template<typename Func>
double measure_ms(Func&& fn, int iters = 3) {
    // Warmup
    fn();
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        fn();
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count() / iters;
}

void run_suite_size(size_t N, int iters) {
    std::cout << "\n========================================================================================================\n";
    std::cout << "  BENCHMARK: STACK BUFFER BUDGET COMPARISON (N = " << N << ", " << iters << " iterations average)\n";
    std::cout << "========================================================================================================\n";
    std::cout << std::left << std::setw(20) << "Distribution"
              << std::right << std::setw(11) << "std::sort"
              << std::setw(12) << "std::stable"
              << std::setw(14) << "GNU in-place"
              << std::setw(14) << "VBlock (0 B)"
              << std::setw(14) << "VBlock (1KB)"
              << std::setw(14) << "VBlock (2KB)"
              << std::setw(14) << "VBlock (4KB)"
              << "\n";
    std::cout << std::string(104, '-') << "\n";

    std::vector<std::pair<std::string, std::vector<int>>> datasets;

    std::mt19937 g(42);

    // 1. Random Uniform
    {
        std::vector<int> a(N);
        std::uniform_int_distribution<int> dist(0, 1000000000);
        for (size_t i = 0; i < N; ++i) a[i] = dist(g);
        datasets.push_back({"Random Uniform", a});
    }

    // 2. Binary (0 and 1)
    {
        std::vector<int> a(N);
        std::uniform_int_distribution<int> dist(0, 1);
        for (size_t i = 0; i < N; ++i) a[i] = dist(g);
        datasets.push_back({"Binary (2 Keys)", a});
    }

    // 3. Pipe Organ
    {
        std::vector<int> a(N);
        size_t mid = N / 2;
        for (size_t i = 0; i < mid; ++i) a[i] = int(i);
        for (size_t i = mid; i < N; ++i) a[i] = int(N - i);
        datasets.push_back({"Pipe Organ", a});
    }

    // 4. Push Front
    {
        std::vector<int> a(N);
        for (size_t i = 0; i < N; ++i) a[i] = int(i);
        a[0] = int(N + 100);
        datasets.push_back({"Push Front", a});
    }

    // 5. Already Sorted
    {
        std::vector<int> a(N);
        for (size_t i = 0; i < N; ++i) a[i] = int(i);
        datasets.push_back({"Already Sorted", a});
    }

    // 6. Strictly Reversed
    {
        std::vector<int> a(N);
        for (size_t i = 0; i < N; ++i) a[i] = int(N - i);
        datasets.push_back({"Reversed", a});
    }

    // 7. Sawtooth (16 ramps)
    {
        std::vector<int> a(N);
        size_t ramp = N / 16;
        for (size_t i = 0; i < N; ++i) a[i] = int(i % ramp);
        datasets.push_back({"Sawtooth (16 ramps)", a});
    }

    for (const auto& [name, master] : datasets) {
        std::vector<int> work = master;

        double t_std_sort = measure_ms([&]() {
            work = master;
            std::sort(work.begin(), work.end());
        }, iters);

        double t_std_stable = measure_ms([&]() {
            work = master;
            std::stable_sort(work.begin(), work.end());
        }, iters);

        double t_gnu = measure_ms([&]() {
            work = master;
            __gnu_test::gnu_inplace_stable_sort(work.begin(), work.end(), std::less<int>());
        }, iters);

        double t_vblock_0 = measure_ms([&]() {
            work = master;
            VBlock::Sort<0>(work.begin(), work.end());
        }, iters);

        double t_vblock_1k = measure_ms([&]() {
            work = master;
            VBlock::Sort<1024>(work.begin(), work.end());
        }, iters);

        double t_vblock_2k = measure_ms([&]() {
            work = master;
            VBlock::Sort<2048>(work.begin(), work.end());
        }, iters);

        double t_vblock_4k = measure_ms([&]() {
            work = master;
            VBlock::Sort<4096>(work.begin(), work.end());
        }, iters);

        std::cout << std::left << std::setw(20) << name
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(9) << t_std_sort << " ms"
                  << std::setw(10) << t_std_stable << " ms"
                  << std::setw(12) << t_gnu << " ms"
                  << std::setw(12) << t_vblock_0 << " ms"
                  << std::setw(12) << t_vblock_1k << " ms"
                  << std::setw(12) << t_vblock_2k << " ms"
                  << std::setw(12) << t_vblock_4k << " ms"
                  << "\n";
    }
}

int main() {
    std::cout << "V-BLOCKSORT STACK BUFFER ABLATION BENCHMARK\n";
    std::cout << "Testing: 0 Byte (Pure In-Place), 1 KB (256 ints), 2 KB (512 ints), 4 KB (1024 ints/max)\n";
    std::cout << "vs GNU __inplace_stable_sort (Pure in-place SymMerge)\n";

    run_suite_size(100000, 5);
    run_suite_size(1000000, 3);

    return 0;
}
