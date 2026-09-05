#ifndef WALL_L_HPP
#define WALL_L_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <utility>

namespace WallL {

template <typename T, typename Compare>
void wall1InsertionSort(T* arr, int start, int end, Compare comp) {
    for (int i = start + 1; i < end; ++i) {
        T key = std::move(arr[i]);
        int j = i - 1;
        while (j >= start && comp(key, arr[j])) {
            arr[j + 1] = std::move(arr[j]);
            j--;
        }
        arr[j + 1] = std::move(key);
    }
}

template <typename T, typename Compare>
void mergeSubarrays(T* arr, int low, int mid, int high, Compare comp) {
    int left_ptr = low;
    int right_ptr = mid;
    std::vector<T> buffer;
    buffer.reserve(high - low);

    while (left_ptr < mid && right_ptr < high) {
        if (!comp(arr[right_ptr], arr[left_ptr])) {
            buffer.push_back(std::move(arr[left_ptr++]));
        } else {
            buffer.push_back(std::move(arr[right_ptr++]));
        }
    }

    while (left_ptr < mid)  buffer.push_back(std::move(arr[left_ptr++]));
    while (right_ptr < high) buffer.push_back(std::move(arr[right_ptr++]));

    for (size_t i = 0; i < buffer.size(); ++i) {
        arr[low + i] = std::move(buffer[i]);
    }
}

template <typename T, typename Compare>
void wallLMergeSortRecursive(T* arr, int start, int end, int current_L, Compare comp) {
    int length = end - start;
    if (length <= 1) return;

    if (current_L <= 1) {
        wall1InsertionSort(arr, start, end, comp);
        return;
    }

    int next_block_size = std::max(2, static_cast<int>(std::pow(length, 1.0 / current_L)));

    for (int i = start; i < end; i += next_block_size) {
        int next_end = std::min(i + next_block_size, end);
        wallLMergeSortRecursive(arr, i, next_end, current_L - 1, comp);
    }

    int merge_width = next_block_size;
    while (merge_width < length) {
        for (int i = start; i < end; i += 2 * merge_width) {
            int low = i;
            int mid = std::min(i + merge_width, end);
            int high = std::min(i + 2 * merge_width, end);

            if (mid < high && comp(arr[mid], arr[mid - 1])) {
                mergeSubarrays(arr, low, mid, high, comp);
            }
        }
        merge_width *= 2;
    }
}

template <typename T, typename Compare>
void sort(T* arr, size_t size, int L, Compare comp) {
    if (L < 1) L = 1;
    wallLMergeSortRecursive(arr, 0, static_cast<int>(size), L, comp);
}

template <typename T>
void sort(T* arr, size_t size, int L = 3) {
    sort(arr, size, L, std::less<T>());
}

} // namespace WallL

#endif // WALL_L_HPP
