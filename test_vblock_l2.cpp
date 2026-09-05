#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "vblock_layer0.hpp"
#include "vblock_layer1.hpp"
#include "vblock_layer2_bench.hpp"

using namespace VBlock;
using namespace VBlockLayer2;

struct Item {
    int64_t key;
    uint32_t id;

    bool operator<(const Item& other) const {
        return key < other.key;
    }
};

// Pure O(1) MergePairAdaptive using SymMerge instead of std::inplace_merge
template <typename T, typename Compare>
void MergePairAdaptive_PureO1(T* a, size_t len_a, size_t len_b, T* buf, size_t buf_cap, Compare comp) {
    if (len_a == 0 || len_b == 0) return;

    T* b = a + len_a;

    // Boundary check 1: Already sorted
    if (!comp(b[0], a[len_a - 1])) return;

    // Boundary check 2: Reversed
    if (comp(b[len_b - 1], a[0])) {
        std::rotate(a, b, b + len_b);
        return;
    }

    // Boundary trim A
    T* a_start = GallopUpperBound(a, a + len_a, b[0], comp);
    if (a_start == a + len_a) return;
    size_t head_skip = std::distance(a, a_start);
    a += head_skip;
    len_a -= head_skip;

    // Boundary trim B
    T* b_end = GallopLowerBound(b, b + len_b, a[len_a - 1], comp);
    len_b = std::distance(b, b_end);
    if (len_b == 0) return;

    if (len_a <= buf_cap) {
        for (size_t i = 0; i < len_a; ++i) buf[i] = std::move(a[i]);
        T* p_buf = buf;
        T* p_buf_end = buf + len_a;
        T* p_b = b;
        T* p_b_end = b + len_b;
        T* dest = a;
        while (p_buf < p_buf_end && p_b < p_b_end) {
            if (!comp(*p_b, *p_buf)) *dest++ = std::move(*p_buf++);
            else *dest++ = std::move(*p_b++);
        }
        while (p_buf < p_buf_end) *dest++ = std::move(*p_buf++);
    } else if (len_b <= buf_cap) {
        for (size_t i = 0; i < len_b; ++i) buf[i] = std::move(b[i]);
        T* p_buf = buf + len_b - 1;
        T* p_a = a + len_a - 1;
        T* dest = b + len_b - 1;
        while (p_buf >= buf && p_a >= a) {
            if (comp(*p_buf, *p_a)) *dest-- = std::move(*p_a--);
            else *dest-- = std::move(*p_buf--);
        }
        while (p_buf >= buf) *dest-- = std::move(*p_buf--);
    } else {
        // Pure O(1) fallback: SymMerge (guaranteed stable, no heap allocation!)
        MergeLowKey_SymMerge(a, 0, len_a, len_a + len_b, comp);
    }
}

// SymBuffer: Divide & conquer with Gallop, leaf-merging using the 512 buffer
template <typename T, typename Compare>
void MergeSymBuffer(T* arr, size_t first, size_t mid, size_t last, T* buf, size_t buf_cap, Compare comp) {
    if (first >= mid || mid >= last) return;

    // Fast check 1: Already sorted
    if (!comp(arr[mid], arr[mid - 1])) return;

    // Fast check 2: Reversed
    if (comp(arr[last - 1], arr[first])) {
        std::rotate(arr + first, arr + mid, arr + last);
        return;
    }

    // Adaptive Gallop Trim Head
    T* a_start = GallopUpperBound(arr + first, arr + mid, arr[mid], comp);
    if (a_start == arr + mid) return;
    first = std::distance(arr, a_start);

    // Adaptive Gallop Trim Tail
    T* b_end = GallopLowerBound(arr + mid, arr + last, arr[mid - 1], comp);
    last = std::distance(arr, b_end);
    if (mid >= last) return;

    size_t len1 = mid - first;
    size_t len2 = last - mid;

    // LEAF BASE CASE 1: First half fits into buffer!
    if (len1 <= buf_cap) {
        for (size_t i = 0; i < len1; ++i) buf[i] = std::move(arr[first + i]);
        T* p_buf = buf;
        T* p_buf_end = buf + len1;
        T* p_b = arr + mid;
        T* p_b_end = arr + last;
        T* dest = arr + first;
        while (p_buf < p_buf_end && p_b < p_b_end) {
            if (!comp(*p_b, *p_buf)) *dest++ = std::move(*p_buf++);
            else *dest++ = std::move(*p_b++);
        }
        while (p_buf < p_buf_end) *dest++ = std::move(*p_buf++);
        return;
    }

    // LEAF BASE CASE 2: Second half fits into buffer!
    if (len2 <= buf_cap) {
        for (size_t i = 0; i < len2; ++i) buf[i] = std::move(arr[mid + i]);
        T* p_buf = buf + len2 - 1;
        T* p_a = arr + mid - 1;
        T* dest = arr + last - 1;
        while (p_buf >= buf && p_a >= arr + first) {
            if (comp(*p_buf, *p_a)) *dest-- = std::move(*p_a--);
            else *dest-- = std::move(*p_buf--);
        }
        while (p_buf >= buf) *dest-- = std::move(*p_buf--);
        return;
    }

    // Split
    size_t m1, m2;
    if (len1 >= len2) {
        m1 = first + len1 / 2;
        m2 = std::distance(arr, GallopLowerBound(arr + mid, arr + last, arr[m1], comp));
    } else {
        m2 = mid + len2 / 2;
        m1 = std::distance(arr, GallopUpperBound(arr + first, arr + mid, arr[m2], comp));
    }

    std::rotate(arr + m1, arr + mid, arr + m2);
    size_t new_mid = m1 + (m2 - mid);

    MergeSymBuffer(arr, first, m1, new_mid, buf, buf_cap, comp);
    MergeSymBuffer(arr, new_mid, m2, last, buf, buf_cap, comp);
}

template <typename T, typename Compare>
void SortLayer2_PureO1(T* arr, size_t n, Compare comp) {
    if (n <= 16) {
        SortMicroBlocks(arr, n, comp);
        return;
    }

    SortMicroBlocks(arr, n, comp);

    constexpr size_t CACHE_SIZE = 512;
    T cache[CACHE_SIZE];

    size_t run_len = 16;
    while (run_len < n) {
        size_t double_run = run_len * 2;
        for (size_t i = 0; i < n; i += double_run) {
            if (i + run_len < n) {
                size_t len_a = run_len;
                size_t len_b = std::min(run_len, n - (i + run_len));
                MergePairAdaptive_PureO1(arr + i, len_a, len_b, cache, CACHE_SIZE, comp);
            }
        }
        run_len = double_run;
    }
}

template <typename T, typename Compare>
void SortLayer2_SymBuffer(T* arr, size_t n, Compare comp) {
    if (n <= 16) {
        SortMicroBlocks(arr, n, comp);
        return;
    }

    SortMicroBlocks(arr, n, comp);

    constexpr size_t CACHE_SIZE = 512;
    T cache[CACHE_SIZE];

    size_t run_len = 16;
    while (run_len < n) {
        size_t double_run = run_len * 2;
        for (size_t i = 0; i < n; i += double_run) {
            if (i + run_len < n) {
                size_t len_a = run_len;
                size_t len_b = std::min(run_len, n - (i + run_len));
                MergeSymBuffer(arr + i, 0, len_a, len_a + len_b, cache, CACHE_SIZE, comp);
            }
        }
        run_len = double_run;
    }
}

bool verify_sorted_and_stable(const std::vector<Item>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i].key < arr[i - 1].key) return false;
        if (arr[i].key == arr[i - 1].key && arr[i].id < arr[i - 1].id) return false;
    }
    return true;
}

int main() {
    const size_t N = 100000;
    std::mt19937 rng(1337);

    struct Scenario {
        std::string name;
        std::vector<Item> data;
    };

    std::vector<Scenario> scenarios = {
        {"Random Uniform", {}},
        {"Binary Keys (2 Keys)", {}},
        {"4 Keys", {}},
        {"Sqrt(N) Keys (~316 Keys)", {}},
        {"Already Sorted", {}},
        {"Reverse Sorted", {}},
        {"V-Shape (Organ Pipe)", {}}
    };

    scenarios[0].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[0].data[i] = {static_cast<int64_t>(rng() % 1000000), static_cast<uint32_t>(i)};

    scenarios[1].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[1].data[i] = {static_cast<int64_t>(rng() % 2), static_cast<uint32_t>(i)};

    scenarios[2].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[2].data[i] = {static_cast<int64_t>(rng() % 4), static_cast<uint32_t>(i)};

    scenarios[3].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[3].data[i] = {static_cast<int64_t>(rng() % 316), static_cast<uint32_t>(i)};

    scenarios[4].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[4].data[i] = {static_cast<int64_t>(i), static_cast<uint32_t>(i)};

    scenarios[5].data.resize(N);
    for (size_t i = 0; i < N; ++i) scenarios[5].data[i] = {static_cast<int64_t>(N - i), static_cast<uint32_t>(i)};

    scenarios[6].data.resize(N);
    for (size_t i = 0; i < N / 2; ++i) scenarios[6].data[i] = {static_cast<int64_t>(N / 2 - i), static_cast<uint32_t>(i)};
    for (size_t i = N / 2; i < N; ++i) scenarios[6].data[i] = {static_cast<int64_t>(i - N / 2), static_cast<uint32_t>(i)};

    std::cout << "\n=== COMPARISON: Layer 1 vs Pure SymMerge vs SymBuffer (Strict O(1)) ===\n";
    std::cout << std::left  << std::setw(26) << "Scenario"
              << std::right << std::setw(13) << "L1 (ms)"
              << std::right << std::setw(15) << "SymMerge (ms)"
              << std::right << std::setw(15) << "SymBuffer (ms)"
              << std::right << std::setw(14) << "Status"
              << "\n" << std::string(83, '-') << "\n";

    for (const auto& sc : scenarios) {
        // Test L1
        std::vector<Item> arr1 = sc.data;
        auto t0 = std::chrono::high_resolution_clock::now();
        SortLayer1(arr1.data(), N, [](const Item& a, const Item& b) { return a.key < b.key; });
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms1 = std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Test L2 Pure
        std::vector<Item> arr2 = sc.data;
        auto t2 = std::chrono::high_resolution_clock::now();
        SortLayer2_PureO1(arr2.data(), N, [](const Item& a, const Item& b) { return a.key < b.key; });
        auto t3 = std::chrono::high_resolution_clock::now();
        double ms2 = std::chrono::duration<double, std::milli>(t3 - t2).count();
        bool ok2 = verify_sorted_and_stable(arr2);

        // Test L2 SymBuffer
        std::vector<Item> arr3 = sc.data;
        auto t4 = std::chrono::high_resolution_clock::now();
        SortLayer2_SymBuffer(arr3.data(), N, [](const Item& a, const Item& b) { return a.key < b.key; });
        auto t5 = std::chrono::high_resolution_clock::now();
        double ms3 = std::chrono::duration<double, std::milli>(t5 - t4).count();
        bool ok3 = verify_sorted_and_stable(arr3);

        std::cout << std::left  << std::setw(26) << sc.name
                  << std::right << std::setw(13) << std::fixed << std::setprecision(2) << ms1
                  << std::right << std::setw(15) << std::fixed << std::setprecision(2) << ms2
                  << std::right << std::setw(15) << std::fixed << std::setprecision(2) << ms3
                  << std::right << std::setw(14) << (ok3 ? "PASS (STABLE)" : "FAIL")
                  << "\n";
    }

    return 0;
}
