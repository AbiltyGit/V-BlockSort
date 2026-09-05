#ifndef VBLOCK_SORT_PURE_LEGACY_HPP
#define VBLOCK_SORT_PURE_LEGACY_HPP

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <iterator>
#include <functional>

namespace VBlockLegacy {

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

template <typename RandomAccessIterator, typename T, typename Compare>
inline RandomAccessIterator GallopLowerBound(RandomAccessIterator first, RandomAccessIterator last,
                                             const T& val, Compare comp) {
    size_t count = std::distance(first, last);
    if (count == 0) return first;
    size_t step = 1;
    size_t curr = 0;
    while (curr < count && comp(first[curr], val)) {
        size_t next = curr + step;
        if (next >= count) {
            return std::lower_bound(first + curr, last, val, comp);
        }
        if (!comp(first[next], val)) {
            return std::lower_bound(first + curr, first + next, val, comp);
        }
        curr = next;
        step <<= 1;
    }
    return first + curr;
}

template <typename RandomAccessIterator, typename T, typename Compare>
inline RandomAccessIterator GallopUpperBound(RandomAccessIterator first, RandomAccessIterator last,
                                             const T& val, Compare comp) {
    size_t count = std::distance(first, last);
    if (count == 0) return first;
    size_t step = 1;
    size_t curr = 0;
    while (curr < count && !comp(val, first[curr])) {
        size_t next = curr + step;
        if (next >= count) {
            return std::upper_bound(first + curr, last, val, comp);
        }
        if (comp(val, first[next])) {
            return std::upper_bound(first + curr, first + next, val, comp);
        }
        curr = next;
        step <<= 1;
    }
    return first + curr;
}

template <typename T, typename Compare>
void MergeSymBuffer(T* arr, size_t first, size_t mid, size_t last, T* buf, size_t buf_cap, Compare comp) {
    if (first >= mid || mid >= last) return;

    if (!comp(arr[mid], arr[mid - 1])) return;

    if (comp(arr[last - 1], arr[first])) {
        std::rotate(arr + first, arr + mid, arr + last);
        return;
    }

    T* a_start = GallopUpperBound(arr + first, arr + mid, arr[mid], comp);
    if (a_start == arr + mid) return;
    first = std::distance(arr, a_start);

    T* b_end = GallopLowerBound(arr + mid, arr + last, arr[mid - 1], comp);
    last = std::distance(arr, b_end);
    if (mid >= last) return;

    size_t len1 = mid - first;
    size_t len2 = last - mid;

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
    MergeSymBuffer(arr, new_mid + 1, m2, last, buf, buf_cap, comp);
}

template <size_t MaxStackBytes = 4096, typename T, typename Compare>
void Sort(T* arr, size_t n, Compare comp) {
    if (n <= 1) return;
    if (n <= 16) {
        SortMicroBlocks(arr, n, comp);
        return;
    }

    SortMicroBlocks(arr, n, comp);

    constexpr size_t RAW_CAP = MaxStackBytes / sizeof(T);
    constexpr size_t CACHE_SIZE = (sizeof(T) <= MaxStackBytes)
                                  ? (RAW_CAP > 512 ? 512 : RAW_CAP)
                                  : 0;

    if constexpr (CACHE_SIZE > 0) {
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
        size_t run_len = 16;
        while (run_len < n) {
            size_t double_run = run_len * 2;
            for (size_t i = 0; i < n; i += double_run) {
                if (i + run_len < n) {
                    size_t len_a = run_len;
                    size_t len_b = std::min(run_len, n - (i + run_len));
                    MergeSymBuffer(arr + i, 0, len_a, len_a + len_b, static_cast<T*>(nullptr), 0, comp);
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

} // namespace VBlockLegacy

#endif // VBLOCK_SORT_PURE_LEGACY_HPP
