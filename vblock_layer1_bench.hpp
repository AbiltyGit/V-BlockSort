#ifndef VBLOCK_LAYER1_BENCH_HPP
#define VBLOCK_LAYER1_BENCH_HPP

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <utility>

namespace VBlockLayer1 {

// Global counters for ablation metrics
struct Metrics {
    uint64_t comparisons = 0;
    uint64_t element_moves = 0;

    void reset() {
        comparisons = 0;
        element_moves = 0;
    }
};

inline thread_local Metrics g_metrics;

// =========================================================================
// SECTION 1: SEARCH STRATEGIES
// =========================================================================

// 1.1: Linear Search
template <typename RandomAccessIterator, typename T, typename Compare>
RandomAccessIterator SearchLinear(RandomAccessIterator first, RandomAccessIterator last,
                                 const T& value, Compare comp) {
    while (first != last) {
        g_metrics.comparisons++;
        if (!comp(*first, value)) {
            return first;
        }
        ++first;
    }
    return last;
}

// 1.2: Standard Binary Search
template <typename RandomAccessIterator, typename T, typename Compare>
RandomAccessIterator SearchBinary(RandomAccessIterator first, RandomAccessIterator last,
                                 const T& value, Compare comp) {
    size_t len = std::distance(first, last);
    while (len > 0) {
        size_t half = len >> 1;
        RandomAccessIterator middle = first + half;
        g_metrics.comparisons++;
        if (comp(*middle, value)) {
            first = middle + 1;
            len = len - half - 1;
        } else {
            len = half;
        }
    }
    return first;
}

// 1.3: WikiSort Galloping Search (Adaptive forward jump then binary search)
template <typename RandomAccessIterator, typename T, typename Compare>
RandomAccessIterator SearchGallop(RandomAccessIterator first, RandomAccessIterator last,
                                 const T& value, Compare comp, size_t step_hint = 8) {
    size_t size = std::distance(first, last);
    if (size == 0) return first;

    size_t skip = std::max(size_t(1), step_hint);
    RandomAccessIterator index = first;

    // Gallop forward with step size
    while (index + skip < last) {
        index += skip;
        g_metrics.comparisons++;
        if (!comp(*(index - 1), value)) {
            // Target is in [index - skip, index)
            return SearchBinary(index - skip, index, value, comp);
        }
        skip <<= 1; // Exponential galloping
    }

    // Binary search remaining range [index - (skip >> 1), last)
    return SearchBinary(index, last, value, comp);
}

// =========================================================================
// SECTION 2: BLOCK REARRANGEMENT / MOVEMENT STRATEGIES
// =========================================================================

// Helper for tracked block swap
template <typename T>
inline void TrackedSwapRanges(T* a, T* b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        std::swap(a[i], b[i]);
        g_metrics.element_moves += 2;
    }
}

template <typename T>
inline void TrackedCopyRange(const T* src, T* dst, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
        g_metrics.element_moves++;
    }
}

// 2.1: Rotation-based Block Rolling (WikiSort Stili)
// When placing block i into target position, rotates the span
template <typename T, typename Compare>
void BlockMoveRotation(T* arr, size_t num_blocks, size_t block_len,
                       const std::vector<size_t>& target_positions, Compare comp) {
    for (size_t i = 0; i < num_blocks; ++i) {
        size_t current_idx = i;
        size_t target_idx = target_positions[i];
        if (current_idx != target_idx && target_idx > current_idx) {
            size_t start = current_idx * block_len;
            size_t mid   = (current_idx + 1) * block_len;
            size_t end   = (target_idx + 1) * block_len;
            
            // Track element moves during std::rotate
            size_t total_elems = end - start;
            g_metrics.element_moves += total_elems; // std::rotate moves elements
            std::rotate(arr + start, arr + mid, arr + end);
        }
    }
}

// 2.2: Tagged Selection Sort on Blocks (KotaSort blockSelect)
template <typename T, typename Compare>
void BlockMoveSelect(T* arr, size_t num_blocks, size_t block_len,
                     std::vector<size_t>& tags, Compare comp) {
    for (size_t j = 0; j < num_blocks; ++j) {
        size_t start = j * block_len;
        size_t min_block = j;

        for (size_t i = j + 1; i < num_blocks; ++i) {
            g_metrics.comparisons++;
            if (tags[i] < tags[min_block]) {
                min_block = i;
            }
        }

        if (min_block != j) {
            TrackedSwapRanges(arr + start, arr + min_block * block_len, block_len);
            std::swap(tags[j], tags[min_block]);
        }
    }
}

// 2.3: Cycle Sort on Blocks (Optimal O(K) block swaps)
template <typename T, typename Compare>
void BlockMoveCycle(T* arr, size_t num_blocks, size_t block_len,
                    std::vector<size_t>& tags, T* aux_buf, Compare comp) {
    for (size_t i = 0; i < num_blocks; ++i) {
        while (tags[i] != i) {
            size_t dest = tags[i];
            TrackedSwapRanges(arr + i * block_len, arr + dest * block_len, block_len);
            std::swap(tags[i], tags[dest]);
        }
    }
}

// =========================================================================
// SECTION 3: ADAPTIVE BOUNDARY CHECKS
// =========================================================================
enum class BoundaryResult {
    ALREADY_MERGED, // A[end-1] <= B[0]
    NEEDS_REVERSE,  // B[end-1] < A[0]
    NEEDS_MERGE     // Overlapping keys
};

template <typename T, typename Compare>
inline BoundaryResult CheckBoundaries(const T* a_start, const T* a_end,
                                      const T* b_start, const T* b_end, Compare comp) {
    g_metrics.comparisons++;
    if (!comp(*b_start, *(a_end - 1))) {
        // a_end - 1 <= b_start : perfectly sorted!
        return BoundaryResult::ALREADY_MERGED;
    }

    g_metrics.comparisons++;
    if (comp(*(b_end - 1), *a_start)) {
        // b_end - 1 < a_start : reverse order, single rotation suffices!
        return BoundaryResult::NEEDS_REVERSE;
    }

    return BoundaryResult::NEEDS_MERGE;
}

} // namespace VBlockLayer1

#endif // VBLOCK_LAYER1_BENCH_HPP
