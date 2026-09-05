#ifndef VBLOCK_LAYER0_HPP
#define VBLOCK_LAYER0_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <algorithm>

namespace VBlock {

// -------------------------------------------------------------------------
// Helper: Branchless conditional swap
// -------------------------------------------------------------------------
template <typename T>
inline void compare_swap_branchless(T& a, T& b) noexcept {
    bool swap_needed = b < a;
    T min_val = swap_needed ? b : a;
    T max_val = swap_needed ? a : b;
    a = min_val;
    b = max_val;
}

template <typename T, typename Compare>
inline void compare_swap_branchless(T& a, T& b, Compare comp) noexcept {
    bool swap_needed = comp(b, a);
    T min_val = swap_needed ? b : a;
    T max_val = swap_needed ? a : b;
    a = min_val;
    b = max_val;
}

// -------------------------------------------------------------------------
// Candidate 1: 16-Element Index-Tagged Branchless Network (WikiSort Style)
// -------------------------------------------------------------------------
template <typename T, typename Compare>
inline void compare_swap_tagged(T& a, T& b, uint8_t& ord_a, uint8_t& ord_b, Compare comp) noexcept {
    bool less_ba = comp(b, a);
    bool eq = !less_ba && !comp(a, b);
    bool swap_needed = less_ba || (eq && ord_a > ord_b);

    T min_val = swap_needed ? b : a;
    T max_val = swap_needed ? a : b;
    uint8_t min_ord = swap_needed ? ord_b : ord_a;
    uint8_t max_ord = swap_needed ? ord_a : ord_b;

    a = min_val;
    b = max_val;
    ord_a = min_ord;
    ord_b = max_ord;
}

template <typename T, typename Compare>
void sort16_network_tagged(T* arr, Compare comp) noexcept {
    uint8_t ord[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    #define CS_TAG(i, j) compare_swap_tagged(arr[i], arr[j], ord[i], ord[j], comp)

    // Layer 1
    CS_TAG(0, 1);   CS_TAG(2, 3);   CS_TAG(4, 5);   CS_TAG(6, 7);
    CS_TAG(8, 9);   CS_TAG(10, 11); CS_TAG(12, 13); CS_TAG(14, 15);

    // Layer 2
    CS_TAG(0, 2);   CS_TAG(1, 3);   CS_TAG(4, 6);   CS_TAG(5, 7);
    CS_TAG(8, 10);  CS_TAG(9, 11);  CS_TAG(12, 14); CS_TAG(13, 15);

    // Layer 3
    CS_TAG(1, 2);   CS_TAG(5, 6);   CS_TAG(9, 10);  CS_TAG(13, 14);
    CS_TAG(0, 4);   CS_TAG(1, 5);   CS_TAG(2, 6);   CS_TAG(3, 7);
    CS_TAG(8, 12);  CS_TAG(9, 13);  CS_TAG(10, 14); CS_TAG(11, 15);

    // Layer 4
    CS_TAG(2, 4);   CS_TAG(3, 5);   CS_TAG(10, 12); CS_TAG(11, 13);
    CS_TAG(1, 4);   CS_TAG(3, 6);   CS_TAG(9, 12);  CS_TAG(11, 14);
    CS_TAG(2, 3);   CS_TAG(6, 7);   CS_TAG(10, 11); CS_TAG(14, 15);

    // Layer 5 (Global Butterfly)
    CS_TAG(0, 8);   CS_TAG(1, 9);   CS_TAG(2, 10);  CS_TAG(3, 11);
    CS_TAG(4, 12);  CS_TAG(5, 13);  CS_TAG(6, 14);  CS_TAG(7, 15);

    // Merge Layers
    CS_TAG(4, 8);   CS_TAG(5, 9);   CS_TAG(6, 10);  CS_TAG(7, 11);
    CS_TAG(2, 8);   CS_TAG(3, 9);   CS_TAG(6, 12);  CS_TAG(7, 13);
    CS_TAG(1, 8);   CS_TAG(3, 10);  CS_TAG(5, 12);  CS_TAG(7, 14);
    CS_TAG(2, 4);   CS_TAG(3, 5);   CS_TAG(6, 8);   CS_TAG(7, 9);   CS_TAG(10, 12); CS_TAG(11, 13);
    CS_TAG(1, 2);   CS_TAG(3, 4);   CS_TAG(5, 6);   CS_TAG(7, 8);   CS_TAG(9, 10);  CS_TAG(11, 12); CS_TAG(13, 14);

    #undef CS_TAG
}

// -------------------------------------------------------------------------
// Candidate 2: 16-Element Odd-Even Transposition Network (Inherently Stable)
// -------------------------------------------------------------------------
template <typename T, typename Compare>
void sort16_oddeven_branchless(T* arr, Compare comp) noexcept {
    #define CS(i, j) compare_swap_branchless(arr[i], arr[j], comp)

    // 16 alternating rounds of even/odd adjacent comparisons
    // Perfectly branchless, preserves relative order of equal keys naturally
    #pragma GCC unroll 8
    for (int round = 0; round < 8; ++round) {
        // Even phase (adjacent pairs starting at 0)
        CS(0, 1);   CS(2, 3);   CS(4, 5);   CS(6, 7);
        CS(8, 9);   CS(10, 11); CS(12, 13); CS(14, 15);

        // Odd phase (adjacent pairs starting at 1)
        CS(1, 2);   CS(3, 4);   CS(5, 6);   CS(7, 8);
        CS(9, 10);  CS(11, 12); CS(13, 14);
    }

    #undef CS
}

// -------------------------------------------------------------------------
// Candidate 3: Unrolled Insertion Sort with Branchless Selection
// -------------------------------------------------------------------------
template <typename T, typename Compare>
void sort16_unrolled_insertion(T* arr, Compare comp) noexcept {
    for (int i = 1; i < 16; ++i) {
        T key = arr[i];
        int j = i - 1;
        while (j >= 0 && comp(key, arr[j])) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// -------------------------------------------------------------------------
// Candidate 6: Straight Unrolled Insertion Sort (Compile-time unrolled)
// -------------------------------------------------------------------------
template <typename T, typename Compare>
void sort16_straight_unroll(T* arr, Compare comp) noexcept {
    #define INSERT_STEP(i) { \
        T key = arr[i]; \
        int j = i - 1; \
        while (j >= 0 && comp(key, arr[j])) { \
            arr[j + 1] = arr[j]; \
            --j; \
        } \
        arr[j + 1] = key; \
    }

    INSERT_STEP(1);
    INSERT_STEP(2);
    INSERT_STEP(3);
    INSERT_STEP(4);
    INSERT_STEP(5);
    INSERT_STEP(6);
    INSERT_STEP(7);
    INSERT_STEP(8);
    INSERT_STEP(9);
    INSERT_STEP(10);
    INSERT_STEP(11);
    INSERT_STEP(12);
    INSERT_STEP(13);
    INSERT_STEP(14);
    INSERT_STEP(15);

    #undef INSERT_STEP
}

// -------------------------------------------------------------------------
// Candidate 4: Branchless Binary Insertion Sort (Fixed N=16)
// -------------------------------------------------------------------------
template <typename T, typename Compare>
void sort16_binary_insertion(T* arr, Compare comp) noexcept {
    for (int i = 1; i < 16; ++i) {
        T key = arr[i];
        // Binary search upper_bound for stability
        int left = 0, right = i;
        while (left < right) {
            int mid = left + (right - left) / 2;
            if (comp(key, arr[mid])) {
                right = mid;
            } else {
                left = mid + 1;
            }
        }
        // Shift elements
        for (int j = i; j > left; --j) {
            arr[j] = arr[j - 1];
        }
        arr[left] = key;
    }
}

// -------------------------------------------------------------------------
// Candidate 5: 8-Element Network (WikiSort Native)
// -------------------------------------------------------------------------
template <typename T, typename Compare>
void sort8_network_tagged(T* arr, Compare comp) noexcept {
    uint8_t ord[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    #define CS8(i, j) compare_swap_tagged(arr[i], arr[j], ord[i], ord[j], comp)

    CS8(0, 1); CS8(2, 3); CS8(4, 5); CS8(6, 7);
    CS8(0, 2); CS8(1, 3); CS8(4, 6); CS8(5, 7);
    CS8(1, 2); CS8(5, 6); CS8(0, 4); CS8(3, 7);
    CS8(1, 5); CS8(2, 6);
    CS8(1, 4); CS8(3, 6);
    CS8(2, 4); CS8(3, 5);
    CS8(3, 4);

    #undef CS8
}

} // namespace VBlock

#endif // VBLOCK_LAYER0_HPP
