#ifndef VBLOCK_LAYER1_HPP
#define VBLOCK_LAYER1_HPP

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include "vblock_layer0.hpp"

namespace VBlock {

// =========================================================================
// 1. ADAPTIVE GALLOPING SEARCH
// =========================================================================

// Find first element in [first, last) that is NOT less than value (lower_bound)
template <typename RandomAccessIterator, typename T, typename Compare>
inline RandomAccessIterator GallopLowerBound(RandomAccessIterator first, RandomAccessIterator last,
                                            const T& value, Compare comp) {
    size_t size = std::distance(first, last);
    if (size == 0) return first;

    // Small range: Linear search is fastest on CPU
    if (size <= 16) {
        while (first != last && comp(*first, value)) {
            ++first;
        }
        return first;
    }

    // Gallop forward with exponential step
    size_t step = 1;
    RandomAccessIterator prev = first;
    RandomAccessIterator curr = first;

    while (curr < last && comp(*curr, value)) {
        prev = curr;
        curr += step;
        step <<= 1;
    }

    if (curr > last) curr = last;

    // Standard binary search on narrowed window [prev, curr)
    return std::lower_bound(prev, curr, value, comp);
}

// Find first element in [first, last) that is strictly GREATER than value (upper_bound)
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
// 2. LAYER 0 DISPATCHER (16-Element Micro-Kernel)
// =========================================================================

template <typename T, typename Compare>
inline void SortMicroBlocks(T* arr, size_t n, Compare comp) {
    size_t full_blocks = n / 16;
    for (size_t b = 0; b < full_blocks; ++b) {
        sort16_straight_unroll(arr + b * 16, comp);
    }

    // Remainder tail (< 16 elements)
    size_t rem = n % 16;
    if (rem > 1) {
        T* tail = arr + full_blocks * 16;
        for (size_t i = 1; i < rem; ++i) {
            T key = tail[i];
            size_t j = i;
            while (j > 0 && comp(key, tail[j - 1])) {
                tail[j] = tail[j - 1];
                --j;
            }
            tail[j] = key;
        }
    }
}

// =========================================================================
// 3. LAYER 1 MERGE WITH GALLOPING & ADAPTIVE BOUNDARY EARLY-EXIT
// =========================================================================

// Merge two adjacent sorted ranges [a, a + len_a) and [b, b + len_b) using buffer
// where b = a + len_a.
template <typename T, typename Compare>
void MergePairAdaptive(T* a, size_t len_a, size_t len_b, T* buf, size_t buf_cap, Compare comp) {
    if (len_a == 0 || len_b == 0) return;

    T* b = a + len_a;

    // --- Adaptive Boundary Checks ---
    // 1. Already sorted: a[len_a - 1] <= b[0]
    if (!comp(b[0], a[len_a - 1])) {
        return; // Zero operations needed!
    }

    // 2. Completely reversed: b[len_b - 1] < a[0]
    if (comp(b[len_b - 1], a[0])) {
        std::rotate(a, b, b + len_b);
        return;
    }

    // 3. Gallop to trim leading elements of A that are already in place
    T* a_start = GallopUpperBound(a, a + len_a, b[0], comp);
    if (a_start == a + len_a) {
        // All elements in A are <= b[0]
        return;
    }
    size_t head_skip = std::distance(a, a_start);
    a += head_skip;
    len_a -= head_skip;

    // 4. Gallop to trim trailing elements of B that are already in place
    T* b_end = GallopLowerBound(b, b + len_b, a[len_a - 1], comp);
    len_b = std::distance(b, b_end);
    if (len_b == 0) {
        return;
    }

    // Now merge trimmed [a, a + len_a) and [b, b + len_b)
    if (len_a <= buf_cap) {
        // Copy smaller range A into buf
        for (size_t i = 0; i < len_a; ++i) {
            buf[i] = std::move(a[i]);
        }

        T* p_buf = buf;
        T* p_buf_end = buf + len_a;
        T* p_b = b;
        T* p_b_end = b + len_b;
        T* dest = a;

        while (p_buf < p_buf_end && p_b < p_b_end) {
            if (!comp(*p_b, *p_buf)) {
                *dest++ = std::move(*p_buf++);
            } else {
                *dest++ = std::move(*p_b++);
            }
        }

        while (p_buf < p_buf_end) {
            *dest++ = std::move(*p_buf++);
        }
    } else if (len_b <= buf_cap) {
        // Backward merge using B in buffer
        for (size_t i = 0; i < len_b; ++i) {
            buf[i] = std::move(b[i]);
        }

        T* p_buf = buf + len_b - 1;
        T* p_a = a + len_a - 1;
        T* dest = b + len_b - 1;

        while (p_buf >= buf && p_a >= a) {
            if (comp(*p_buf, *p_a)) {
                *dest-- = std::move(*p_a--);
            } else {
                *dest-- = std::move(*p_buf--);
            }
        }

        while (p_buf >= buf) {
            *dest-- = std::move(*p_buf--);
        }
    } else {
        // In-place rotation merge fallback if both exceed cache
        std::inplace_merge(a, b, b + len_b, comp);
    }
}

// =========================================================================
// 4. LAYER 1 HIERARCHICAL RUN ENGINE
// =========================================================================

template <typename T, typename Compare>
void SortLayer1(T* arr, size_t n, Compare comp) {
    if (n <= 16) {
        SortMicroBlocks(arr, n, comp);
        return;
    }

    // Step 1: Base micro-block sorting (N=16 blocks)
    SortMicroBlocks(arr, n, comp);

    // Step 2: Fixed 512-element cache (O(1) memory)
    constexpr size_t CACHE_SIZE = 512;
    T cache[CACHE_SIZE];

    // Step 3: Hierarchical bottom-up merge with adaptive galloping
    size_t run_len = 16;
    while (run_len < n) {
        size_t double_run = run_len * 2;
        for (size_t i = 0; i < n; i += double_run) {
            if (i + run_len < n) {
                size_t len_a = run_len;
                size_t len_b = std::min(run_len, n - (i + run_len));
                MergePairAdaptive(arr + i, len_a, len_b, cache, CACHE_SIZE, comp);
            }
        }
        run_len = double_run;
    }
}

template <typename T>
inline void SortLayer1(T* arr, size_t n) {
    SortLayer1(arr, n, std::less<T>());
}

} // namespace VBlock

#endif // VBLOCK_LAYER1_HPP
