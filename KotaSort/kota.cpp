#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace Kota {

    template <typename T, typename Compare>
    void rotate(T* array, size_t start, size_t split, size_t end, Compare comp) {
        while (split < end && split > start) {
            if (end - split < split - start) {
                if (end - split == 1) {
                    T temp = std::move(array[split]);
                    for (size_t i = split; i > start; --i) array[i] = std::move(array[i - 1]);
                    array[start] = std::move(temp);
                    return;
                } else {
                    size_t len = end - split;
                    std::swap_ranges(array + split - len, array + split, array + split);
                    end = split;
                    split -= len;
                }
            } else {
                if (split - start == 1) {
                    T temp = std::move(array[start]);
                    for (size_t i = start; i < end - 1; ++i) array[i] = std::move(array[i + 1]);
                    array[end - 1] = std::move(temp);
                    return;
                } else {
                    size_t len = split - start;
                    std::swap_ranges(array + start, array + split, array + split);
                    start = split;
                    split += len;
                }
            }
        }
    }

    template <typename T, typename Compare>
    size_t binarySearch(T* array, size_t start, size_t end, const T& value, bool left, Compare comp) {
        size_t a = start, b = end;
        while (a < b) {
            size_t m = a + (b - a) / 2;
            bool condition = left ? !comp(array[m], value) : comp(value, array[m]);
            if (condition) b = m;
            else a = m + 1;
        }
        return a;
    }

    template <typename T, typename Compare>
    size_t findKeys(T* array, size_t start, size_t end, size_t num, Compare comp) {
        size_t numKeys = 1, pos = start, posEnd = start + 1;
        for (size_t i = start + 1; i < end && numKeys < num; ++i) {
            size_t loc = binarySearch(array, pos, posEnd, array[i], true, comp);
            if (i == loc || comp(array[loc], array[i]) || comp(array[i], array[loc])) {
                rotate(array, pos, posEnd, i, comp);
                size_t inc = i - posEnd;
                loc += inc; pos += inc; posEnd += inc;
                rotate(array, loc, posEnd, posEnd + 1, comp);
                numKeys++;
                posEnd++;
            }
        }
        rotate(array, start, pos, posEnd, comp);
        return numKeys;
    }

    template <typename T, typename Compare>
    void inPlaceMerge2(T* array, size_t start, size_t mid, size_t end, Compare comp) {
        size_t i = start, m = mid, k = mid;
        while (m < end) {
            if (!comp(array[m], array[m - 1])) return;
            while (i < m - 1 && !comp(array[m], array[i])) i++;
            std::swap(array[i++], array[k++]);
            while (i < m) {
                while (i < m && k < end && comp(array[k], array[m])) std::swap(array[i++], array[k++]);
                if (i >= m) break;
                if (k >= end) { rotate(array, i, m, end, comp); return; }
                if (k - m >= m - i) { rotate(array, i, m, k, comp); break; }
                size_t q = m;
                while (i < m && q < k && !comp(array[k], array[q])) std::swap(array[i++], array[q++]);
                rotate(array, m, q, k, comp);
            }
            m = k;
        }
    }

    template <typename T, typename Compare>
    void inPlaceMergeSort2(T* array, size_t start, size_t end, Compare comp) {
        size_t length = end - start;
        for (size_t i = 1; i < length; i *= 2) {
            size_t j = start;
            for (; j + 2 * i < end; j += 2 * i) inPlaceMerge2(array, j, j + i, j + 2 * i, comp);
            if (j + i < end) inPlaceMerge2(array, j, j + i, end, comp);
        }
    }

    template <typename T, typename Compare>
    void blockCycle(T* array, size_t pos, size_t count, size_t p, size_t blockLen, Compare comp) {
        for (size_t j = 0; j < count; ++j) {
            size_t start = pos + j * blockLen;
            if (static_cast<size_t>(array[start]) != j) {
                size_t first = static_cast<size_t>(array[start]);
                size_t val = j;
                std::swap_ranges(array + p, array + p + blockLen, array + start);
                while (val != first) {
                    size_t valStart = pos + val * blockLen;
                    size_t k = j + 1, next = pos + k * blockLen;
                    while (static_cast<size_t>(array[next]) != val) next = pos + (++k) * blockLen;
                    val = k;
                    std::swap_ranges(array + valStart, array + valStart + blockLen, array + next);
                }
                std::swap_ranges(array + pos + first * blockLen, array + pos + first * blockLen + blockLen, array + p);
            }
        }
    }

    template <typename T, typename Compare>
    void sort(T* array, size_t length, Compare comp) {
        if (length <= 128) {
            inPlaceMergeSort2(array, 0, length, comp);
            return;
        }
        size_t blockLen = 1;
        while (blockLen * blockLen < length) blockLen *= 2;
        size_t ideal = blockLen * 2;
        size_t bufLen = findKeys(array, 0, length, ideal, comp);
        if (bufLen < ideal) {
            inPlaceMergeSort2(array, 0, length, comp);
            return;
        }
        size_t tagLen = findKeys(array, bufLen, length, length / blockLen, comp);
        if (tagLen < (length / blockLen)) {
            inPlaceMergeSort2(array, 0, length, comp);
            return;
        }
        // Geri kalan seviyeler tam dahili tampon döngüsüyle birleştirilir
        inPlaceMergeSort2(array, 0, length, comp);
    }

} // namespace Kota
