#ifndef VBLOCK_LAYER2_BENCH_HPP
#define VBLOCK_LAYER2_BENCH_HPP

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <utility>
#include <cmath>

namespace VBlockLayer2 {

struct Metrics {
    uint64_t comparisons = 0;
    uint64_t element_moves = 0;

    void reset() {
        comparisons = 0;
        element_moves = 0;
    }
};

inline thread_local Metrics g_metrics;

template <typename T>
inline void TrackedRotate(T* first, T* mid, T* last) {
    size_t count = std::distance(first, last);
    g_metrics.element_moves += count;
    std::rotate(first, mid, last);
}

template <typename T>
inline void TrackedSwap(T& a, T& b) {
    g_metrics.element_moves += 2;
    std::swap(a, b);
}

// =========================================================================
// SECTION 1: KEY EXTRACTION STRATEGIES
// =========================================================================

// 1.1: Grail / Kota Style (Scan & Rotate)
template <typename T, typename Compare>
size_t ExtractKeys_ScanRotate(T* arr, size_t n, size_t target_keys, Compare comp) {
    if (n <= target_keys || target_keys <= 1) return 1;

    size_t num_keys = 1;
    size_t pos = 0;
    size_t pos_end = 1;

    for (size_t i = 1; i < n && num_keys < target_keys; ++i) {
        // Binary search in [pos, pos_end)
        size_t left = pos, right = pos_end;
        while (left < right) {
            size_t mid = left + (right - left) / 2;
            g_metrics.comparisons++;
            if (!comp(arr[mid], arr[i])) { // arr[mid] >= arr[i]
                right = mid;
            } else {
                left = mid + 1;
            }
        }

        // If arr[i] is a new key (not equal to arr[left])
        bool is_new = true;
        if (left < pos_end) {
            g_metrics.comparisons++;
            if (!comp(arr[left], arr[i]) && !comp(arr[i], arr[left])) {
                is_new = false;
            }
        }

        if (is_new) {
            TrackedRotate(arr + pos, arr + pos_end, arr + i);
            size_t inc = i - pos_end;
            left += inc;
            pos += inc;
            pos_end += inc;
            TrackedRotate(arr + left, arr + pos_end, arr + pos_end + 1);
            num_keys++;
            pos_end++;
        }
    }

    TrackedRotate(arr, arr + pos, arr + pos_end);
    return num_keys;
}

// 1.2: WikiSort Bidirectional Galloping Pull
template <typename T, typename Compare>
size_t ExtractKeys_GallopingPull(T* arr, size_t n, size_t target_keys, Compare comp) {
    if (n <= target_keys || target_keys <= 1) return 1;

    // Pull from left edge
    size_t found = 1;
    size_t last_idx = 0;

    for (size_t i = 1; i < n && found < target_keys; ++i) {
        g_metrics.comparisons++;
        if (comp(arr[last_idx], arr[i]) || comp(arr[i], arr[last_idx])) {
            // New distinct key found
            if (i != found) {
                TrackedRotate(arr + found, arr + i, arr + i + 1);
            }
            last_idx = found;
            found++;
        }
    }
    return found;
}

// 1.3: V-Block Adaptive Budgeted Key Extraction (Fast Bailout)
template <typename T, typename Compare>
size_t ExtractKeys_AdaptiveBudget(T* arr, size_t n, size_t target_keys, Compare comp) {
    if (n <= target_keys || target_keys <= 1) return 1;

    size_t num_keys = 1;
    size_t pos = 0;
    size_t pos_end = 1;

    // Budget: If we scan budget_limit elements without finding enough keys, bail out!
    // For target_keys, we allow scanning at most 8 * target_keys elements
    size_t budget_limit = std::min(n, target_keys * 8);

    for (size_t i = 1; i < budget_limit && num_keys < target_keys; ++i) {
        // Binary search in [pos, pos_end)
        size_t left = pos, right = pos_end;
        while (left < right) {
            size_t mid = left + (right - left) / 2;
            g_metrics.comparisons++;
            if (!comp(arr[mid], arr[i])) { // arr[mid] >= arr[i]
                right = mid;
            } else {
                left = mid + 1;
            }
        }

        // If arr[i] is a new key (not equal to arr[left])
        bool is_new = true;
        if (left < pos_end) {
            g_metrics.comparisons++;
            if (!comp(arr[left], arr[i]) && !comp(arr[i], arr[left])) {
                is_new = false;
            }
        }

        if (is_new) {
            TrackedRotate(arr + pos, arr + pos_end, arr + i);
            size_t inc = i - pos_end;
            left += inc;
            pos += inc;
            pos_end += inc;
            TrackedRotate(arr + left, arr + pos_end, arr + pos_end + 1);
            num_keys++;
            pos_end++;
        }
    }

    // Restore key buffer to front of array
    TrackedRotate(arr, arr + pos, arr + pos_end);
    return num_keys;
}

// =========================================================================
// SECTION 2: LOW-KEY MERGE STRATEGIES (FALLBACK SHIELDS)
// =========================================================================

// 2.1: KotaSort's Quadratic inPlaceMerge2 (Negative Control)
template <typename T, typename Compare>
void MergeLowKey_KotaFallback(T* arr, size_t start, size_t mid, size_t end, Compare comp) {
    size_t i = start, m = mid, k = mid;
    while (m < end) {
        g_metrics.comparisons++;
        if (!comp(arr[m], arr[m - 1])) return;

        while (i < m - 1) {
            g_metrics.comparisons++;
            if (comp(arr[m], arr[i])) break;
            i++;
        }
        TrackedSwap(arr[i++], arr[k++]);

        while (i < m) {
            while (i < m && k < end) {
                g_metrics.comparisons++;
                if (!comp(arr[k], arr[m])) break;
                TrackedSwap(arr[i++], arr[k++]);
            }
            if (i >= m) break;
            if (k >= end) {
                TrackedRotate(arr + i, arr + m, arr + end);
                return;
            }
            if (k - m >= m - i) {
                TrackedRotate(arr + i, arr + m, arr + k);
                break;
            }

            size_t q = m;
            while (i < m && q < k) {
                g_metrics.comparisons++;
                if (comp(arr[k], arr[q])) break;
                TrackedSwap(arr[i++], arr[q++]);
            }
            TrackedRotate(arr + m, arr + q, arr + k);
        }
        m = k;
    }
}

// 2.2: SymMerge (Dudzinski & Dydek / Kim & Kutzner In-Place Stable Merge)
// Divides and conquers without any auxiliary buffer, guaranteed 100% stable
template <typename T, typename Compare>
void MergeLowKey_SymMerge(T* arr, size_t first, size_t mid, size_t last, Compare comp) {
    if (first >= mid || mid >= last) return;

    // Fast check: already sorted
    g_metrics.comparisons++;
    if (!comp(arr[mid], arr[mid - 1])) return;

    size_t len1 = mid - first;
    size_t len2 = last - mid;

    if (len1 == 1) {
        // Find upper_bound of arr[first] in [mid, last)
        size_t left = mid, right = last;
        while (left < right) {
            size_t m = left + (right - left) / 2;
            g_metrics.comparisons++;
            if (comp(arr[m], arr[first])) {
                left = m + 1;
            } else {
                right = m;
            }
        }
        TrackedRotate(arr + first, arr + mid, arr + left);
        return;
    }

    if (len2 == 1) {
        // Find lower_bound of arr[mid] in [first, mid)
        size_t left = first, right = mid;
        while (left < right) {
            size_t m = left + (right - left) / 2;
            g_metrics.comparisons++;
            if (!comp(arr[mid], arr[m])) { // arr[m] <= arr[mid]
                left = m + 1;
            } else {
                right = m;
            }
        }
        TrackedRotate(arr + left, arr + mid, arr + last);
        return;
    }

    size_t m1, m2;
    if (len1 >= len2) {
        m1 = first + len1 / 2;
        // Find lower_bound of arr[m1] in [mid, last)
        size_t left = mid, right = last;
        while (left < right) {
            size_t m = left + (right - left) / 2;
            g_metrics.comparisons++;
            if (comp(arr[m], arr[m1])) {
                left = m + 1;
            } else {
                right = m;
            }
        }
        m2 = left;
    } else {
        m2 = mid + len2 / 2;
        // Find upper_bound of arr[m2] in [first, mid)
        size_t left = first, right = mid;
        while (left < right) {
            size_t m = left + (right - left) / 2;
            g_metrics.comparisons++;
            if (!comp(arr[m2], arr[m])) { // arr[m] <= arr[m2]
                left = m + 1;
            } else {
                right = m;
            }
        }
        m1 = left;
    }

    // Rotate A2 and B1: [m1, mid) and [mid, m2)
    TrackedRotate(arr + m1, arr + mid, arr + m2);
    size_t new_mid = m1 + (m2 - mid);

    // Subproblem 1: [first, m1) and [m1, new_mid)
    MergeLowKey_SymMerge(arr, first, m1, new_mid, comp);
    // Subproblem 2: [new_mid, m2) and [m2, last)
    MergeLowKey_SymMerge(arr, new_mid, m2, last, comp);
}

// 2.3: Run-Length Galloping In-Place Merge (V-Block Low-Key Shield)
// Finds entire runs of equal/smaller keys with Gallop and performs single bulk rotations
template <typename T, typename Compare>
void MergeLowKey_RunLengthGallop(T* arr, size_t a, size_t m, size_t b, Compare comp) {
    size_t i = a;
    size_t j = m;

    while (i < j && j < b) {
        // If arr[i] <= arr[j], find how many elements in range A are <= arr[j]
        g_metrics.comparisons++;
        if (!comp(arr[j], arr[i])) {
            // Gallop forward in range A
            size_t step = 1;
            size_t next_i = i + 1;
            while (next_i < j) {
                g_metrics.comparisons++;
                if (comp(arr[j], arr[next_i])) break;
                i = next_i;
                next_i += step;
                step <<= 1;
            }
            if (next_i > j) next_i = j;

            // Binary search in [i, next_i)
            while (i < next_i) {
                size_t mid = i + (next_i - i) / 2;
                g_metrics.comparisons++;
                if (!comp(arr[j], arr[mid])) {
                    i = mid + 1;
                } else {
                    next_i = mid;
                }
            }
            if (i >= j) break;
        }

        // Now arr[i] > arr[j]. Find how many elements in range B are < arr[i]
        size_t step = 1;
        size_t next_j = j + 1;
        while (next_j < b) {
            g_metrics.comparisons++;
            if (!comp(arr[next_j], arr[i])) break;
            j = next_j;
            next_j += step;
            step <<= 1;
        }
        if (next_j > b) next_j = b;

        // Binary search in [j, next_j)
        size_t b_target = j;
        size_t b_lim = next_j;
        while (b_target < b_lim) {
            size_t mid = b_target + (b_lim - b_target) / 2;
            g_metrics.comparisons++;
            if (comp(arr[mid], arr[i])) {
                b_target = mid + 1;
            } else {
                b_lim = mid;
            }
        }

        // Bulk rotate run of B into position i
        TrackedRotate(arr + i, arr + j, arr + b_target);
        i += (b_target - j);
        j = b_target;
    }
}

} // namespace VBlockLayer2

#endif // VBLOCK_LAYER2_BENCH_HPP
