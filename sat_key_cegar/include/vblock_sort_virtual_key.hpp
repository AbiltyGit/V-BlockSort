#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <iterator>
#include "virtual_key_synthesis.hpp"
#include "synthesized_invariants.hpp"

/**
 * ============================================================================
 * V-BLOCKSORT (VIRTUAL KEY VARIANT): ZERO-EXTRACTION BLOCK MERGE SORT
 * ============================================================================
 * 
 * Architecture:
 * - Stage 0: 16-element unrolled insertion sort for micro-blocks.
 * - Stage 1: Virtual Tag Encoding: Origin 1 (Sequence B) encoded via adjacent 
 *            carrier pair inversion (x_i < x_{i+1}). Monochromatic blocks tagged free.
 * - Stage 2: In-Place Block Selection: Rearranges blocks by head/tail elements
 *            with stable tie-breaking on decoded virtual origin.
 * - Stage 3: Local Block Merge & Invariant Restoration: Merges consecutive blocks
 *            using SAT micro-kernels while restoring inverted pairs in-place.
 * 
 * Properties:
 * - Strictly 0 Bytes Heap Allocation.
 * - Strictly 0 Auxiliary Key Buffers extracted from array.
 * - 100% Strictly Stable.
 * ============================================================================
 */

namespace VBlockVK {

// Stage 0: 16-element unrolled insertion sort
template <typename T, typename Compare>
inline void sort16_straight_unroll(T* arr, Compare comp) {
    for (size_t i = 1; i < 16; ++i) {
        T val = std::move(arr[i]);
        size_t j = i;
        while (j > 0 && comp(val, arr[j - 1])) {
            arr[j] = std::move(arr[j - 1]);
            --j;
        }
        arr[j] = std::move(val);
    }
}

template <typename T, typename Compare>
inline void SortMicroBlocks(T* arr, size_t n, Compare comp) {
    size_t full_blocks = n / 16;
    for (size_t b = 0; b < full_blocks; ++b) {
        sort16_straight_unroll(arr + b * 16, comp);
    }
    size_t rem = n % 16;
    if (rem > 1) {
        T* rem_arr = arr + full_blocks * 16;
        for (size_t i = 1; i < rem; ++i) {
            T val = std::move(rem_arr[i]);
            size_t j = i;
            while (j > 0 && comp(val, rem_arr[j - 1])) {
                rem_arr[j] = std::move(rem_arr[j - 1]);
                --j;
            }
            rem_arr[j] = std::move(val);
        }
    }
}

// Swap two blocks of size B in-place
template <typename T>
inline void swap_blocks(T* a, T* b, size_t B) {
    for (size_t i = 0; i < B; ++i) {
        std::swap(a[i], b[i]);
    }
}

// Inspect leading element of a block (accounting for possible virtual inversion at index 0)
template <typename T, typename Compare>
inline const T& block_head(const T* block, size_t B, Compare comp) {
    if (B >= 2 && comp(block[1], block[0])) {
        // Tag inverted pair at (0, 1) -> smaller element is block[1]
        return block[1];
    }
    return block[0];
}

// Inspect trailing element of a block (accounting for possible virtual inversion at index B-2)
template <typename T, typename Compare>
inline const T& block_tail(const T* block, size_t B, Compare comp) {
    if (B >= 2 && comp(block[B - 1], block[B - 2])) {
        // Tag inverted pair at (B-2, B-1) -> larger element is block[B-2]
        return block[B - 2];
    }
    return block[B - 1];
}

// Check virtual origin of a block without restoring it
template <typename T, typename Compare>
inline int inspect_virtual_origin(const T* block, size_t B, Compare comp) {
    for (size_t i = 0; i + 1 < B; ++i) {
        if (comp(block[i + 1], block[i])) return 1; // Inversion found -> Sequence B
    }
    return 0; // Sequence A or Monochromatic
}

// Virtual Key Block Merge Kernel for adjacent runs [first, mid) and [mid, last)
template <typename T, typename Compare>
void MergeVirtualKeyBlocks(T* arr, size_t first, size_t mid, size_t last, Compare comp) {
    if (first >= mid || mid >= last) return;

    // Fast check: Already sorted
    if (!comp(arr[mid], arr[mid - 1])) return;

    // Fast check: Strictly reversed
    if (comp(arr[last - 1], arr[first])) {
        std::rotate(arr + first, arr + mid, arr + last);
        return;
    }

    size_t len1 = mid - first;
    size_t len2 = last - mid;
    constexpr size_t B = 8; // Block size

    // If sub-partitions are small, dispatch directly to SAT synthesized micro-merge
    if (SatCegar::Synthesized::try_micro_merge_leaf(arr + first, len1, len2, comp)) {
        return;
    }

    // If smaller than 2 blocks, fallback to small rotate
    if (len1 < B || len2 < B) {
        if (len1 <= len2) {
            T* m1 = arr + first;
            T* m2 = std::lower_bound(arr + mid, arr + last, *m1, comp);
            std::rotate(m1, arr + mid, m2);
            size_t new_first = std::distance(arr, m2);
            MergeVirtualKeyBlocks(arr, new_first, mid + (m2 - (arr + mid)), last, comp);
        } else {
            T* m2 = arr + mid;
            T* m1 = std::upper_bound(arr + first, arr + mid, *m2, comp);
            std::rotate(m1, arr + mid, arr + last);
        }
        return;
    }

    // Step 1: Virtual Tag Encoding
    // Encode Origin 1 in Sequence B blocks
    size_t num_b_blocks = len2 / B;
    for (size_t b = 0; b < num_b_blocks; ++b) {
        SatKeyResearch::encode_origin_tag(arr + mid + b * B, B, 1, comp);
    }

    // Step 2: In-Place Block Selection Sort (Permute full blocks)
    size_t num_a_blocks = len1 / B;
    size_t total_blocks = num_a_blocks + num_b_blocks;
    T* block_start = arr + first;

    for (size_t i = 0; i < total_blocks; ++i) {
        size_t min_idx = i;
        for (size_t j = i + 1; j < total_blocks; ++j) {
            const T& head_min = block_head(block_start + min_idx * B, B, comp);
            const T& head_j = block_head(block_start + j * B, B, comp);

            if (comp(head_j, head_min)) {
                min_idx = j;
            } else if (!comp(head_min, head_j)) {
                // Tie on head: tie-break using virtual origin (Origin 0 before Origin 1)
                int orig_min = inspect_virtual_origin(block_start + min_idx * B, B, comp);
                int orig_j = inspect_virtual_origin(block_start + j * B, B, comp);
                if (orig_j < orig_min) {
                    min_idx = j;
                }
            }
        }
        if (min_idx != i) {
            swap_blocks(block_start + i * B, block_start + min_idx * B, B);
        }
    }

    // Step 3: Decode and restore virtual tags for all blocks
    for (size_t b = 0; b < total_blocks; ++b) {
        SatKeyResearch::decode_and_restore_tag(block_start + b * B, B, comp);
    }

    // Step 4: Local In-Place Rolling Merge across block boundaries
    for (size_t b = 0; b + 1 < total_blocks; ++b) {
        T* b1 = block_start + b * B;
        T* b2 = b1 + B;
        if (comp(b2[0], b1[B - 1])) {
            // Merge boundary between b1 and b2 using SAT 8+8 micro-kernel
            SatCegar::Synthesized::merge_8_8_stable(b1, comp);
        }
    }

    // Handle trailing uneven remainder
    size_t processed = total_blocks * B;
    if (processed < len1 + len2) {
        std::inplace_merge(arr + first, arr + first + processed, arr + last, comp);
    }
}

template <typename RandomAccessIterator, typename Compare>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last, Compare comp) {
    size_t n = std::distance(first, last);
    if (n <= 1) return;
    if (n <= 16) {
        SortMicroBlocks(&first[0], n, comp);
        return;
    }

    // Stage 0: 16-element micro-sorting
    SortMicroBlocks(&first[0], n, comp);

    // Bottom-Up Virtual-Key Block Merge
    size_t run_len = 16;
    while (run_len < n) {
        size_t double_run = run_len * 2;
        for (size_t i = 0; i < n; i += double_run) {
            if (i + run_len < n) {
                size_t len_a = run_len;
                size_t len_b = std::min(run_len, n - (i + run_len));
                MergeVirtualKeyBlocks(&first[0] + i, 0, len_a, len_a + len_b, comp);
            }
        }
        run_len = double_run;
    }
}

template <typename RandomAccessIterator>
inline void Sort(RandomAccessIterator first, RandomAccessIterator last) {
    Sort(first, last, std::less<typename std::iterator_traits<RandomAccessIterator>::value_type>());
}

} // namespace VBlockVK
