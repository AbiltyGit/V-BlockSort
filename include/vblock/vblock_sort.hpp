#ifndef VBLOCK_VBLOCK_SORT_HPP
#define VBLOCK_VBLOCK_SORT_HPP

/**
 * ============================================================================
 * V-BlockSort: High-Performance, Stable, In-Place Hybrid Sorting Algorithm
 * ============================================================================
 * 
 * Architecture:
 * - Katman 0: Compile-time unrolled straight-insertion micro-kernel (N <= 16).
 * - Katman 1: Branch-efficient Galloping search (exponential step lower/upper bound)
 *             with adaptive boundary skips (O(1) comparison on sorted/reversed data).
 * - Katman 2: Low-Key Shield via SymMerge divide-and-conquer (prevents KotaSort O(N^2)).
 * - Katman 3: Byte-Budgeted Stack Buffer (defaults to 4 KB, fitting inside L1 cache;
 *             strictly bounds stack consumption to prevent stack overflow on large structs).
 * 
 * Properties:
 * - 100% Strictly Stable (preserves relative order of equivalent keys).
 * - Zero Dynamic Heap Allocations (0 B heap memory, unlike std::stable_sort).
 * - Strictly Bounded Stack Footprint (<= MaxStackBytes, default 4096 B).
 * - Standard C++17 Header-Only Library.
 * 
 * License: MIT License
 * ============================================================================
 */

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <iterator>
#include <functional>

namespace VBlock {

// =========================================================================
// SECTION 1: MICRO-KERNEL BASE (Katman 0: 16-Element Unrolled Kernel)
// =========================================================================

template <typename T, typename Compare>
inline void sort16_straight_unroll(T* arr, Compare comp) {
    #define VBLOCK_INSERT_STEP(i) { \
        T key = std::move(arr[i]); \
        size_t j = i; \
        while (j > 0 && comp(key, arr[j - 1])) { \
            arr[j] = std::move(arr[j - 1]); \
            --j; \
        } \
        arr[j] = std::move(key); \
    }

    VBLOCK_INSERT_STEP(1);
    VBLOCK_INSERT_STEP(2);
    VBLOCK_INSERT_STEP(3);
    VBLOCK_INSERT_STEP(4);
    VBLOCK_INSERT_STEP(5);
    VBLOCK_INSERT_STEP(6);
    VBLOCK_INSERT_STEP(7);
    VBLOCK_INSERT_STEP(8);
    VBLOCK_INSERT_STEP(9);
    VBLOCK_INSERT_STEP(10);
    VBLOCK_INSERT_STEP(11);
    VBLOCK_INSERT_STEP(12);
    VBLOCK_INSERT_STEP(13);
    VBLOCK_INSERT_STEP(14);
    VBLOCK_INSERT_STEP(15);

    #undef VBLOCK_INSERT_STEP
}

template <typename T, typename Compare>
inline void SortMicroBlocks(T* arr, size_t n, Compare comp) {
    size_t full_blocks = n / 16;
    for (size_t b = 0; b < full_blocks; ++b) {
        sort16_straight_unroll(arr + b * 16, comp);
    }

    size_t rem = n % 16;
    if (rem > 1) {
        T* tail = arr + full_blocks * 16;
        for (size_t i = 1; i < rem; ++i) {
            T key = std::move(tail[i]);
            size_t j = i;
            while (j > 0 && comp(key, tail[j - 1])) {
                tail[j] = std::move(tail[j - 1]);
                --j;
            }
            tail[j] = std::move(key);
        }
    }
}

// =========================================================================
// SECTION 2: ADAPTIVE GALLOPING SEARCH (Katman 1)
// =========================================================================

template <typename RandomAccessIterator, typename T, typename Compare>
inline RandomAccessIterator GallopLowerBound(RandomAccessIterator first, RandomAccessIterator last,
                                            const T& value, Compare comp) {
    size_t size = std::distance(first, last);
    if (size == 0) return first;

    if (size <= 16) {
        while (first != last && comp(*first, value)) {
            ++first;
        }
        return first;
    }

    size_t step = 1;
    RandomAccessIterator prev = first;
    RandomAccessIterator curr = first;

    while (curr < last && comp(*curr, value)) {
        prev = curr;
        curr += step;
        step <<= 1;
    }

    if (curr > last) curr = last;
    return std::lower_bound(prev, curr, value, comp);
}

template <typename RandomAccessIterator, typename T, typename Compare>
inline RandomAccessIterator GallopUpperBound(RandomAccessIterator first, RandomAccessIterator last,
                                            const T& value, Compare comp) {
    size_t size = std::distance(first, last);
    if (size == 0) return first;

    if (size <= 16) {
        while (first != last && !comp(value, *first)) {
            ++first;
        }
        return first;
    }

    size_t step = 1;
    RandomAccessIterator prev = first;
    RandomAccessIterator curr = first;

    while (curr < last && !comp(value, *curr)) {
        prev = curr;
        curr += step;
        step <<= 1;
    }

    if (curr > last) curr = last;
    return std::upper_bound(prev, curr, value, comp);
}

// =========================================================================
// SECTION 3: SYMMETRIC BUFFER MERGE & LOW-KEY SHIELD (Katman 2 & 3)
// =========================================================================

template <typename T, typename Compare>
void MergeSymBuffer(T* arr, size_t first, size_t mid, size_t last, T* buf, size_t buf_cap, Compare comp) {
    if (first >= mid || mid >= last) return;

    // Fast check 1: Already sorted (1 comparison early-exit)
    if (!comp(arr[mid], arr[mid - 1])) return;

    // Fast check 2: Reversed runs (1 comparison + single rotate)
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

    // LEAF BASE CASES: Use buffer if available and partition fits
    if (buf != nullptr && buf_cap > 0) {
        // Forward buffer merge
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

        // Backward buffer merge
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
    }

    // Symmetric Galloping Split (SymMerge divide & conquer)
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

// =========================================================================
// SECTION 4: PUBLIC API & ENGINE DRIVERS
// =========================================================================

/**
 * @brief Sorts [arr, arr + n) stably in-place with bounded stack consumption.
 * 
 * @tparam MaxStackBytes Maximum stack buffer budget in bytes (default: 4096 = 4 KB).
 * @param arr Pointer to first element.
 * @param n Number of elements.
 * @param comp Binary predicate comparator.
 */
template <size_t MaxStackBytes = 4096, typename T, typename Compare>
void Sort(T* arr, size_t n, Compare comp) {
    if (n <= 1) return;
    if (n <= 16) {
        SortMicroBlocks(arr, n, comp);
        return;
    }

    // Stage 0: 16-Element Micro-Kernel Sorting
    SortMicroBlocks(arr, n, comp);

    // Compute type-safe stack buffer capacity
    constexpr size_t RAW_CAP = MaxStackBytes / sizeof(T);
    constexpr size_t CACHE_SIZE = (sizeof(T) <= MaxStackBytes)
                                  ? (RAW_CAP > 512 ? 512 : RAW_CAP)
                                  : 0;

    if constexpr (CACHE_SIZE > 0) {
        // Stack footprint guaranteed <= MaxStackBytes (e.g. 4 KB)
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
    } else {
        // Zero-stack fallback: Pure in-place SymMerge (0 bytes stack array)
        size_t run_len = 16;
        while (run_len < n) {
            size_t double_run = run_len * 2;
            for (size_t i = 0; i < n; i += double_run) {
                if (i + run_len < n) {
                    size_t len_a = run_len;
                    size_t len_b = std::min(run_len, n - (i + run_len));
                    MergeSymBuffer(arr + i, 0, len_a, len_a + len_b, nullptr, 0, comp);
                }
            }
            run_len = double_run;
        }
    }
}

template <size_t MaxStackBytes = 4096, typename RandomAccessIterator, typename Compare>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    size_t n = std::distance(first, last);
    if (n <= 1) return;
    Sort<MaxStackBytes>(&first[0], n, comp);
}

template <size_t MaxStackBytes = 4096, typename RandomAccessIterator>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last) {
    Sort<MaxStackBytes>(first, last, std::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
}

} // namespace VBlock

#endif // VBLOCK_VBLOCK_SORT_HPP
