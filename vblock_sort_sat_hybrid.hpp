#ifndef VBLOCK_VBLOCK_SORT_SAT_HYBRID_HPP
#define VBLOCK_VBLOCK_SORT_SAT_HYBRID_HPP

/**
 * ============================================================================
 * V-BlockSort SAT-Hybrid: Dual-Mode Architecture (L1 Buffer + SAT 0-Buffer Leaf)
 * ============================================================================
 * 
 * Architecture:
 * - Mode A (Buffer Available, buf_cap > 0): Fast L1-resident streaming linear merge
 * - Mode B (Zero Buffer, buf_cap == 0): Formally verified SAT flat micro-kernels (0-recursion)
 */

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <iterator>
#include <functional>
#include "sat_key_cegar/include/synthesized_invariants.hpp"

namespace VBlockSat {

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

    if (comp(*first, value)) {
        size_t step = 1;
        size_t prev = 0;
        while (step < size && comp(*(first + step), value)) {
            prev = step;
            step = (step << 1) + 1;
        }
        if (step >= size) step = size - 1;
        return std::lower_bound(first + prev, first + step + 1, value, comp);
    }
    return first;
}

template <typename RandomAccessIterator, typename T, typename Compare>
inline RandomAccessIterator GallopUpperBound(RandomAccessIterator first, RandomAccessIterator last,
                                            const T& value, Compare comp) {
    size_t size = std::distance(first, last);
    if (size == 0) return first;

    if (!comp(value, *first)) {
        size_t step = 1;
        size_t prev = 0;
        while (step < size && !comp(value, *(first + step))) {
            prev = step;
            step = (step << 1) + 1;
        }
        if (step >= size) step = size - 1;
        return std::upper_bound(first + prev, first + step + 1, value, comp);
    }
    return first;
}

// =========================================================================
// SECTION 3: SYMMETRIC BUFFER MERGE (DUAL-MODE: L1 BUFFER & SAT 0-BUFFER)
// =========================================================================

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

    // =====================================================================
    // MODE 1: L1 STREAMING STACK BUFFER (buf_cap > 0)
    // High-throughput linear streaming merge with hardware prefetching.
    // =====================================================================
    if (buf != nullptr && buf_cap > 0) {
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
    } else {
        // =================================================================
        // MODE 2: PURE ZERO-BUFFER IN-PLACE MODE (buf_cap == 0 / MaxStackBytes=0)
        // Eliminates std::rotate recursions via 11 synthesized SAT kernels.
        // =================================================================
        if (SatCegar::Synthesized::try_micro_merge_leaf(arr + first, len1, len2, comp)) {
            return;
        }
    }

    if (len1 == 1) {
        T* m2 = GallopLowerBound(arr + mid, arr + last, arr[first], comp);
        std::rotate(arr + first, arr + mid, m2);
        return;
    }
    if (len2 == 1) {
        T* m1 = GallopUpperBound(arr + first, arr + mid, arr[mid], comp);
        std::rotate(m1, arr + mid, arr + last);
        return;
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

template <size_t MaxStackBytes = 4096, typename T, typename Compare>
void Sort(T* arr, size_t n, Compare comp) {
    if (n < 2) return;

    // Small size fast path: <= 16 elements
    if (n <= 16) {
        for (size_t i = 1; i < n; ++i) {
            T key = std::move(arr[i]);
            size_t j = i;
            while (j > 0 && comp(key, arr[j - 1])) {
                arr[j] = std::move(arr[j - 1]);
                --j;
            }
            arr[j] = std::move(key);
        }
        return;
    }

    // Katman 0: Base 16-element unrolled sorting pass
    SortMicroBlocks(arr, n, comp);

    // Compute bounded stack buffer capacity
    constexpr size_t element_size = sizeof(T);
    constexpr size_t buf_cap = (element_size > 0 && element_size <= MaxStackBytes)
                             ? (MaxStackBytes / element_size)
                             : 0;

    alignas(alignof(T)) uint8_t raw_stack_buffer[buf_cap > 0 ? (buf_cap * element_size) : 1];
    T* stack_buf = (buf_cap > 0) ? reinterpret_cast<T*>(raw_stack_buffer) : nullptr;

    // Bottom-up iterative block merging
    for (size_t block_size = 16; block_size < n; block_size <<= 1) {
        for (size_t i = 0; i < n; i += (block_size << 1)) {
            size_t mid = std::min(i + block_size, n);
            size_t last = std::min(i + (block_size << 1), n);
            if (mid < last) {
                MergeSymBuffer(arr, i, mid, last, stack_buf, buf_cap, comp);
            }
        }
    }
}

template <size_t MaxStackBytes = 4096, typename RandomAccessIterator, typename Compare>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    using ValueType = typename std::iterator_traits<RandomAccessIterator>::value_type;
    size_t n = std::distance(first, last);
    if (n < 2) return;
    Sort<MaxStackBytes, ValueType, Compare>(&(*first), n, comp);
}

template <size_t MaxStackBytes = 4096, typename RandomAccessIterator>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last) {
    using ValueType = typename std::iterator_traits<RandomAccessIterator>::value_type;
    Sort<MaxStackBytes>(first, last, std::less<ValueType>());
}

} // namespace VBlockSat

#endif // VBLOCK_VBLOCK_SORT_SAT_HYBRID_HPP
