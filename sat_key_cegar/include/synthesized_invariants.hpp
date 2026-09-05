#pragma once

#include <utility>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace SatCegar::Synthesized {

// Helper for element with 8-bit compact tag
template <typename T, typename Compare>
inline void stable_swap_idx(T* arr, uint8_t* tags, int i, int j, Compare comp) {
    if (comp(arr[j], arr[i]) || (!comp(arr[i], arr[j]) && tags[j] < tags[i])) {
        std::swap(arr[i], arr[j]);
        std::swap(tags[i], tags[j]);
    }
}

// -------------------------------------------------------------
// 2+1 In-Place Stable Merge (2 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_2_1_stable(T* arr, Compare comp) {
    uint8_t tags[3] = {0, 1, 2};
    stable_swap_idx(arr, tags, 1, 2, comp);
    stable_swap_idx(arr, tags, 0, 1, comp);
}

// -------------------------------------------------------------
// 3+1 In-Place Stable Merge (3 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_3_1_stable(T* arr, Compare comp) {
    uint8_t tags[4] = {0, 1, 2, 3};
    stable_swap_idx(arr, tags, 2, 3, comp);
    stable_swap_idx(arr, tags, 1, 2, comp);
    stable_swap_idx(arr, tags, 0, 1, comp);
}

// -------------------------------------------------------------
// 4+1 In-Place Stable Merge (4 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_4_1_stable(T* arr, Compare comp) {
    uint8_t tags[5] = {0, 1, 2, 3, 4};
    stable_swap_idx(arr, tags, 3, 4, comp);
    stable_swap_idx(arr, tags, 2, 3, comp);
    stable_swap_idx(arr, tags, 1, 2, comp);
    stable_swap_idx(arr, tags, 0, 1, comp);
}

// -------------------------------------------------------------
// 2+2 In-Place Stable Merge (3 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_2_2_stable(T* arr, Compare comp) {
    uint8_t tags[4] = {0, 1, 2, 3};
    stable_swap_idx(arr, tags, 0, 2, comp);
    stable_swap_idx(arr, tags, 1, 3, comp);
    stable_swap_idx(arr, tags, 1, 2, comp);
}

// -------------------------------------------------------------
// 3+2 In-Place Stable Merge (5 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_3_2_stable(T* arr, Compare comp) {
    uint8_t tags[5] = {0, 1, 2, 3, 4};
    stable_swap_idx(arr, tags, 1, 4, comp);
    stable_swap_idx(arr, tags, 0, 3, comp);
    stable_swap_idx(arr, tags, 1, 3, comp);
    stable_swap_idx(arr, tags, 2, 4, comp);
    stable_swap_idx(arr, tags, 2, 3, comp);
}

// -------------------------------------------------------------
// 3+3 In-Place Stable Merge (6 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_3_3_stable(T* arr, Compare comp) {
    uint8_t tags[6] = {0, 1, 2, 3, 4, 5};
    stable_swap_idx(arr, tags, 1, 4, comp);
    stable_swap_idx(arr, tags, 0, 3, comp);
    stable_swap_idx(arr, tags, 2, 5, comp);
    stable_swap_idx(arr, tags, 2, 4, comp);
    stable_swap_idx(arr, tags, 1, 3, comp);
    stable_swap_idx(arr, tags, 2, 3, comp);
}

// -------------------------------------------------------------
// 4+2 In-Place Stable Merge (6 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_4_2_stable(T* arr, Compare comp) {
    uint8_t tags[6] = {0, 1, 2, 3, 4, 5};
    stable_swap_idx(arr, tags, 0, 4, comp);
    stable_swap_idx(arr, tags, 3, 5, comp);
    stable_swap_idx(arr, tags, 2, 4, comp);
    stable_swap_idx(arr, tags, 3, 4, comp);
    stable_swap_idx(arr, tags, 1, 3, comp);
    stable_swap_idx(arr, tags, 1, 2, comp);
}

// -------------------------------------------------------------
// 4+3 In-Place Stable Merge (8 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_4_3_stable(T* arr, Compare comp) {
    uint8_t tags[7] = {0, 1, 2, 3, 4, 5, 6};
    stable_swap_idx(arr, tags, 0, 4, comp);
    stable_swap_idx(arr, tags, 3, 5, comp);
    stable_swap_idx(arr, tags, 1, 3, comp);
    stable_swap_idx(arr, tags, 5, 6, comp);
    stable_swap_idx(arr, tags, 2, 5, comp);
    stable_swap_idx(arr, tags, 2, 4, comp);
    stable_swap_idx(arr, tags, 3, 4, comp);
    stable_swap_idx(arr, tags, 1, 2, comp);
}

// -------------------------------------------------------------
// 5+2 In-Place Stable Merge (8 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_5_2_stable(T* arr, Compare comp) {
    uint8_t tags[7] = {0, 1, 2, 3, 4, 5, 6};
    stable_swap_idx(arr, tags, 0, 5, comp);
    stable_swap_idx(arr, tags, 3, 6, comp);
    stable_swap_idx(arr, tags, 4, 6, comp);
    stable_swap_idx(arr, tags, 1, 3, comp);
    stable_swap_idx(arr, tags, 1, 5, comp);
    stable_swap_idx(arr, tags, 2, 5, comp);
    stable_swap_idx(arr, tags, 4, 5, comp);
    stable_swap_idx(arr, tags, 3, 4, comp);
}

// -------------------------------------------------------------
// 4+4 In-Place Stable Merge (9 ops)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_4_4_stable(T* arr, Compare comp) {
    uint8_t tags[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    stable_swap_idx(arr, tags, 0, 4, comp);
    stable_swap_idx(arr, tags, 1, 5, comp);
    stable_swap_idx(arr, tags, 2, 6, comp);
    stable_swap_idx(arr, tags, 3, 7, comp);
    stable_swap_idx(arr, tags, 2, 4, comp);
    stable_swap_idx(arr, tags, 3, 5, comp);
    stable_swap_idx(arr, tags, 1, 2, comp);
    stable_swap_idx(arr, tags, 3, 4, comp);
    stable_swap_idx(arr, tags, 5, 6, comp);
}

// -------------------------------------------------------------
// 8+8 In-Place Stable Merge (25 ops - 4 Parallel ILP Layers)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline void merge_8_8_stable(T* arr, Compare comp) {
    uint8_t tags[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    // Layer 1 (8 parallel pairs)
    stable_swap_idx(arr, tags, 0, 8, comp);
    stable_swap_idx(arr, tags, 1, 9, comp);
    stable_swap_idx(arr, tags, 2, 10, comp);
    stable_swap_idx(arr, tags, 3, 11, comp);
    stable_swap_idx(arr, tags, 4, 12, comp);
    stable_swap_idx(arr, tags, 5, 13, comp);
    stable_swap_idx(arr, tags, 6, 14, comp);
    stable_swap_idx(arr, tags, 7, 15, comp);

    // Layer 2 (4 parallel pairs)
    stable_swap_idx(arr, tags, 4, 8, comp);
    stable_swap_idx(arr, tags, 5, 9, comp);
    stable_swap_idx(arr, tags, 6, 10, comp);
    stable_swap_idx(arr, tags, 7, 11, comp);

    // Layer 3 (6 parallel pairs)
    stable_swap_idx(arr, tags, 2, 4, comp);
    stable_swap_idx(arr, tags, 3, 5, comp);
    stable_swap_idx(arr, tags, 6, 8, comp);
    stable_swap_idx(arr, tags, 7, 9, comp);
    stable_swap_idx(arr, tags, 10, 12, comp);
    stable_swap_idx(arr, tags, 11, 13, comp);

    // Layer 4 (7 parallel pairs)
    stable_swap_idx(arr, tags, 1, 2, comp);
    stable_swap_idx(arr, tags, 3, 4, comp);
    stable_swap_idx(arr, tags, 5, 6, comp);
    stable_swap_idx(arr, tags, 7, 8, comp);
    stable_swap_idx(arr, tags, 9, 10, comp);
    stable_swap_idx(arr, tags, 11, 12, comp);
    stable_swap_idx(arr, tags, 13, 14, comp);
}

// -------------------------------------------------------------
// Complete Micro-Merge Leaf Dispatcher (Symmetric & Asymmetric)
// -------------------------------------------------------------
template <typename T, typename Compare>
inline bool try_micro_merge_leaf(T* arr, size_t len1, size_t len2, Compare comp) {
    if (len1 == 8 && len2 == 8) {
        merge_8_8_stable(arr, comp);
        return true;
    }
    if (len1 == 4 && len2 == 4) {
        merge_4_4_stable(arr, comp);
        return true;
    }
    if (len1 == 3 && len2 == 3) {
        merge_3_3_stable(arr, comp);
        return true;
    }
    if (len1 == 2 && len2 == 2) {
        merge_2_2_stable(arr, comp);
        return true;
    }
    if (len1 == 4 && len2 == 3) {
        merge_4_3_stable(arr, comp);
        return true;
    }
    if (len1 == 5 && len2 == 2) {
        merge_5_2_stable(arr, comp);
        return true;
    }
    if (len1 == 4 && len2 == 2) {
        merge_4_2_stable(arr, comp);
        return true;
    }
    if (len1 == 3 && len2 == 2) {
        merge_3_2_stable(arr, comp);
        return true;
    }
    if (len1 == 4 && len2 == 1) {
        merge_4_1_stable(arr, comp);
        return true;
    }
    if (len1 == 3 && len2 == 1) {
        merge_3_1_stable(arr, comp);
        return true;
    }
    if (len1 == 2 && len2 == 1) {
        merge_2_1_stable(arr, comp);
        return true;
    }
    return false;
}

} // namespace SatCegar::Synthesized
