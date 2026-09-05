#ifndef VBLOCK_SORT_HPP
#define VBLOCK_SORT_HPP

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <iterator>
#include <functional>
#include "vblock_layer0.hpp"
#include "vblock_layer1.hpp"

namespace VBlock {

// =========================================================================
// 3. LOW-KEY SHIELD & SYMMETRIC BUFFER MERGE ENGINE (Katman 2 & 3)
// =========================================================================

// Hierarchical divide-and-conquer merge with galloping split points.
// When either sub-partition fits into the byte-budgeted cache, it merges
// linearly in a single streaming pass (zero O(N log N) rotation overhead).
// If buf is nullptr or buf_cap is 0, gracefully executes pure in-place SymMerge.
template <typename T, typename Compare>
void MergeSymBuffer(T* arr, size_t first, size_t mid, size_t last, T* buf, size_t buf_cap, Compare comp) {
    if (first >= mid || mid >= last) return;

    // Fast check 1: Already sorted
    if (!comp(arr[mid], arr[mid - 1])) return;

    // Fast check 2: Reversed runs
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
        // LEAF BASE CASE 1: First half fits into cache
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

        // LEAF BASE CASE 2: Second half fits into cache (Backward Merge)
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

    // Symmetric Galloping Split
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
// 4. MAIN V-BLOCK SORT DRIVERS (Katman 3)
// =========================================================================

// 4.1: Legacy Fixed-512 Version (For comparison)
template <typename T, typename Compare>
void SortFixed512(T* arr, size_t n, Compare comp) {
    if (n <= 1) return;
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

// 4.2: Production Byte-Budgeted Version (Stack-Safe for Arbitrary Types)
// MaxStackBytes defaults to 4096 bytes (4 KB), safely fitting into L1 cache
// and preventing stack overflow even for multi-kilobyte structs.
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

template <typename RandomAccessIterator, typename Compare>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    size_t n = std::distance(first, last);
    if (n <= 1) return;
    Sort<4096>(&first[0], n, comp);
}

template <typename RandomAccessIterator>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last) {
    Sort(first, last, std::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
}

} // namespace VBlock

#endif // VBLOCK_SORT_HPP
