#ifndef KOTA_SORT_CLEAN_HPP
#define KOTA_SORT_CLEAN_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace KotaClean {

template <typename T, typename Compare>
class Sorter {
private:
    T* array;
    Compare comp;
    int blockLen;
    int bufLen;
    int tagLen;
    int bufPos;
    bool ext;

    inline bool less_than(const T& a, const T& b) {
        return comp(a, b);
    }

    inline bool less_or_equal(const T& a, const T& b) {
        return !comp(b, a);
    }

    inline void kotaSwap(int a, int b, bool aux) {
        if (aux) {
            array[a] = std::move(array[b]);
        } else {
            std::swap(array[a], array[b]);
        }
    }

    void shift(int a, int m, int b, bool left, bool aux) {
        if (left) {
            if (m == b) return;
            while (m > a) {
                --b;
                --m;
                kotaSwap(b, m, aux);
            }
        } else {
            if (m == a) return;
            while (m < b) {
                kotaSwap(a++, m++, aux);
            }
        }
    }

    void rotate(int start, int split, int end) {
        std::rotate(array + start, array + split, array + end);
    }

    void multiSwap(int a, int b, int len, bool aux) {
        for (int i = 0; i < len; ++i) {
            kotaSwap(a + i, b + i, aux);
        }
    }

    void multiSwapBW(int a, int b, int len, bool aux) {
        for (int i = 0; i < len; ++i) {
            kotaSwap(a - i, b - i, aux);
        }
    }

    int binarySearch(int start, int end, const T& value, bool left) {
        int a = start, b = end;
        while (a < b) {
            int m = a + (b - a) / 2;
            bool condition = left ? less_or_equal(value, array[m]) : less_than(value, array[m]);
            if (condition) b = m;
            else a = m + 1;
        }
        return a;
    }

    int findKeys(int start, int end, int num) {
        int numKeys = 1, pos = start, posEnd = start + 1;
        for (int i = start + 1; i < end && numKeys < num; ++i) {
            int loc = binarySearch(pos, posEnd, array[i], true);
            if (i == loc || less_than(array[i], array[loc]) || less_than(array[loc], array[i])) {
                rotate(pos, posEnd, i);
                int inc = i - posEnd;
                loc += inc;
                pos += inc;
                posEnd += inc;
                rotate(loc, posEnd, posEnd + 1);
                numKeys++;
                posEnd++;
            }
        }
        rotate(start, pos, posEnd);
        return numKeys;
    }

    void swapToTags(int a, int i, bool aux) {
        if (aux) {
            // not used in pure in-place KotaSort
        } else {
            kotaSwap(bufPos + i, a, false);
        }
    }

    void blockSelect(int pos, int count) {
        for (int j = 0; j < count; ++j) {
            int start = pos + j * blockLen;
            int minIdx = start;
            for (int i = j + 1; i < count; ++i) {
                int sel = pos + i * blockLen;
                if (less_than(array[sel], array[minIdx])) {
                    minIdx = sel;
                }
            }
            if (start != minIdx) {
                multiSwap(start, minIdx, blockLen, false);
            }
            swapToTags(start, j, false);
        }
    }

    void blockSelectBW(int pos, int count) {
        for (int j = 0; j < count; ++j) {
            int start = pos - j * blockLen;
            int minIdx = start;
            for (int i = j + 1; i < count; ++i) {
                int sel = pos - i * blockLen;
                if (less_than(array[sel], array[minIdx])) {
                    minIdx = sel;
                }
            }
            if (start != minIdx) {
                multiSwapBW(start, minIdx, blockLen, false);
            }
            swapToTags(start, j, false);
        }
    }

    void inPlaceMerge2(int start, int mid, int end) {
        int i = start, m = mid, k = mid, q;
        while (m < end) {
            if (less_or_equal(array[m - 1], array[m])) return;
            while (i < m - 1 && less_or_equal(array[i], array[m])) i++;
            std::swap(array[i++], array[k++]);

            while (i < m) {
                while (i < m && k < end && less_than(array[k], array[m])) {
                    std::swap(array[i++], array[k++]);
                }
                if (i >= m) break;
                else if (k >= end) {
                    rotate(i, m, end);
                    return;
                } else if (k - m >= m - i) {
                    rotate(i, m, k);
                    break;
                }

                q = m;
                while (i < m && q < k && less_or_equal(array[q], array[k])) {
                    std::swap(array[i++], array[q++]);
                }
                rotate(m, q, k);
            }
            m = k;
        }
    }

    void inPlaceMergeSort2(int start, int end) {
        int length = end - start;
        for (int i = 1; i < length; i *= 2) {
            int j = start;
            for (; j + 2 * i < end; j += 2 * i) {
                inPlaceMerge2(j, j + i, j + 2 * i);
            }
            if (j + i < end) {
                inPlaceMerge2(j, j + i, end);
            }
        }
    }

    void inPlaceMerge(int a, int m, int b) {
        int i = a, j = m, k;
        while (i < j && j < b) {
            if (less_than(array[j], array[i])) {
                k = binarySearch(j, b, array[i], true);
                rotate(i, j, k);
                i += k - j;
                j = k;
            } else {
                i++;
            }
        }
    }

    void inPlaceMergeBW(int a, int m, int b) {
        int i = m - 1, j = b - 1, k;
        while (j > i && i >= a) {
            if (less_or_equal(array[j], array[i])) {
                k = binarySearch(a, i + 1, array[j], true);
                rotate(k, i + 1, j + 1);
                j -= (i + 1) - k;
                i = k - 1;
            } else {
                j--;
            }
        }
    }

    void mergeWithBuf(int a, int m, int b, int l) {
        int i = a, j = m, k = a - l;
        while (i < m && j < b) {
            if (less_or_equal(array[i], array[j])) {
                kotaSwap(k++, i++, ext);
            } else {
                kotaSwap(k++, j++, ext);
            }
        }
        while (j < b) {
            kotaSwap(k++, j++, ext);
        }
        shift(k, i, m, false, ext);
    }

    void dualMerge(int a, int m, int b, int l) {
        if (b - m <= l) {
            mergeWithBuf(a, m, b, l);
        } else {
            int i = a, j = m, k = a - l;
            while (k < i && i < m) {
                if (less_or_equal(array[i], array[j])) {
                    kotaSwap(k++, i++, ext);
                } else {
                    kotaSwap(k++, j++, ext);
                }
            }

            if (k < i) {
                shift(j - l, j, b, false, ext);
            } else {
                int i2 = m - 1, j2 = b - 1;
                k = (m - 1) + (b - j);
                while (i2 >= i && j2 >= j) {
                    if (less_than(array[j2], array[i2])) {
                        kotaSwap(k--, i2--, ext);
                    } else {
                        kotaSwap(k--, j2--, ext);
                    }
                }
                while (j2 >= j) {
                    kotaSwap(k--, j2--, ext);
                }
            }
        }
    }

    void dualMergeBW(int a, int m, int b, int l) {
        int i = m - 1, j = b - 1, k = b - 1 + l;
        while (k > j && j >= m) {
            if (less_than(array[j], array[i])) {
                kotaSwap(k--, i--, ext);
            } else {
                kotaSwap(k--, j--, ext);
            }
        }

        if (j < m) {
            shift(a, i + 1, i + 1 + l, true, ext);
        } else {
            int i2 = a, j2 = m;
            i++; j++;
            k = m - (i - a);
            while (i2 < i && j2 < j) {
                if (less_or_equal(array[i2], array[j2])) {
                    kotaSwap(k++, i2++, ext);
                } else {
                    kotaSwap(k++, j2++, ext);
                }
            }
            while (i2 < i) {
                kotaSwap(k++, i2++, ext);
            }
        }
    }

    void mergeWithBufStatic(int a, int m, int b, int p, bool bw) {
        int i, j, k, q;
        if (bw) {
            if (b - m < 1) return;
            i = (b - m) - 1; j = m - 1; k = b - 1;
            while (i >= 0 && j >= a) {
                if (less_or_equal(array[p + i], array[j])) {
                    q = binarySearch(a, j + 1, array[p + i], true);
                    while (j >= q) std::swap(array[k--], array[j--]);
                }
                std::swap(array[k--], array[p + (i--)]);
            }
            while (i >= 0) {
                std::swap(array[k--], array[p + (i--)]);
            }
        } else {
            if (m - a < 1) return;
            i = 0; j = m; k = a;
            while (i < m - a && j < b) {
                if (less_than(array[j], array[p + i])) {
                    q = binarySearch(j, b, array[p + i], true);
                    while (j < q) std::swap(array[k++], array[j++]);
                }
                std::swap(array[k++], array[p + (i++)]);
            }
            while (i < m - a) {
                std::swap(array[k++], array[p + (i++)]);
            }
        }
    }

    void blockMerge(int a, int m, int b, bool auxTag) {
        if (b - m <= 2 * bufLen) {
            dualMerge(a, m, b, bufLen);
            return;
        }

        int i = a, j = m, k, first;
        int leftAD = bufLen, rightAD = 0;
        int left = i - bufLen, right = j;
        int tagCount = 0;

        while (i < m && leftAD >= rightAD) {
            k = 0;
            while (i < m && k < blockLen) {
                if (less_or_equal(array[i], array[j])) {
                    kotaSwap(left++, i++, ext);
                } else {
                    kotaSwap(left++, j++, ext);
                    rightAD++;
                    leftAD--;
                }
                k++;
            }
        }

        int selStart = left;

        while (i < m && j < b) {
            while (i < m && j < b && rightAD > leftAD) {
                first = right;
                k = 0;
                while (i < m && j < b && k < blockLen) {
                    if (less_or_equal(array[i], array[j])) {
                        kotaSwap(right++, i++, ext);
                        rightAD--;
                        leftAD++;
                    } else {
                        kotaSwap(right++, j++, ext);
                    }
                    k++;
                }
                while (i < m && k < blockLen) {
                    kotaSwap(right++, i++, ext);
                    rightAD--;
                    leftAD++;
                    k++;
                }
                while (j < b && k < blockLen) {
                    kotaSwap(right++, j++, ext);
                    k++;
                }

                if (k == blockLen) {
                    swapToTags(first, tagCount++, auxTag);
                } else {
                    shift(first, first + k, b, true, ext);
                    j = b - k;
                    right = first;
                }
            }

            while (i < m && j < b && leftAD >= rightAD) {
                first = left;
                k = 0;
                while (i < m && j < b && k < blockLen) {
                    if (less_or_equal(array[i], array[j])) {
                        kotaSwap(left++, i++, ext);
                    } else {
                        kotaSwap(left++, j++, ext);
                        rightAD++;
                        leftAD--;
                    }
                    k++;
                }
                while (i < m && k < blockLen) {
                    kotaSwap(left++, i++, ext);
                    k++;
                }
                while (j < b && k < blockLen) {
                    kotaSwap(left++, j++, ext);
                    rightAD++;
                    leftAD--;
                    k++;
                }

                if (k == blockLen) {
                    swapToTags(first, tagCount++, auxTag);
                } else {
                    rotate(first, m, right);
                    left += right - m;
                    leftAD = 0;
                }
            }
        }

        if (i >= m && leftAD == blockLen && tagCount > 0) {
            multiSwap(left, right - blockLen, blockLen, ext);
        } else {
            if (i < m) {
                rotate(left, m, right);
                left += right - m;
            }
            shift(left, left + leftAD, right, false, ext);
        }
        if (j < b) {
            shift(j - bufLen, j, b, false, ext);
        }

        blockSelect(selStart, tagCount);
    }

    void blockMergeBW(int a, int m, int b, bool auxTag) {
        int i = m - 1, j = b - 1, k, first;
        int leftAD = 0, rightAD = bufLen;
        int left = i, right = j + bufLen;
        int tagCount = 0;

        while (j >= m && rightAD >= leftAD) {
            k = 0;
            while (j >= m && k < blockLen) {
                if (less_than(array[j], array[i])) {
                    kotaSwap(right--, i--, ext);
                    leftAD++;
                    rightAD--;
                } else {
                    kotaSwap(right--, j--, ext);
                }
                k++;
            }
        }

        int selStart = right;

        while (j >= m && i >= a) {
            while (j >= m && i >= a && leftAD > rightAD) {
                first = left;
                k = 0;
                while (j >= m && i >= a && k < blockLen) {
                    if (less_than(array[j], array[i])) {
                        kotaSwap(left--, i--, ext);
                    } else {
                        kotaSwap(left--, j--, ext);
                        rightAD++;
                        leftAD--;
                    }
                    k++;
                }
                while (j >= m && k < blockLen) {
                    kotaSwap(left--, j--, ext);
                    rightAD++;
                    leftAD--;
                    k++;
                }
                while (i >= a && k < blockLen) {
                    kotaSwap(left--, i--, ext);
                    k++;
                }

                if (k == blockLen) {
                    swapToTags(first, tagCount++, auxTag);
                } else {
                    shift(a, first + 1 - k, first + 1, false, ext);
                    i = a - 1 + k;
                    left = first;
                }
            }

            while (j >= m && i >= a && rightAD >= leftAD) {
                first = right;
                k = 0;
                while (j >= m && i >= a && k < blockLen) {
                    if (less_than(array[j], array[i])) {
                        kotaSwap(right--, i--, ext);
                        leftAD++;
                        rightAD--;
                    } else {
                        kotaSwap(right--, j--, ext);
                    }
                    k++;
                }
                while (j >= m && k < blockLen) {
                    kotaSwap(right--, j--, ext);
                    k++;
                }
                while (i >= a && k < blockLen) {
                    kotaSwap(right--, i--, ext);
                    leftAD++;
                    rightAD--;
                    k++;
                }

                if (k == blockLen) {
                    swapToTags(first, tagCount++, auxTag);
                } else {
                    rotate(left + 1, m, first + 1);
                    right -= m - (left + 1);
                    rightAD = 0;
                }
            }
        }

        if (j < m && rightAD == blockLen && tagCount > 0) {
            multiSwapBW(right, left + blockLen, blockLen, ext);
        } else {
            if (j >= m) {
                rotate(left + 1, m, right + 1);
                right -= m - (left + 1);
            }
            shift(left + 1, right + 1 - rightAD, right + 1, true, ext);
        }
        if (i >= a) {
            shift(a, i + 1, i + 1 + bufLen, true, ext);
        }

        blockSelectBW(selStart, tagCount);
    }

    bool kotaIterator(int start, int end, bool auxTag) {
        int i = 1, j, effStart = start + bufLen, length = end - effStart;

        if (!ext) {
            while (i < 16) {
                for (j = effStart; j + 2 * i < end; j += 2 * i) {
                    inPlaceMerge2(j, j + i, j + 2 * i);
                }
                if (j + i < end) {
                    inPlaceMerge2(j, j + i, end);
                }
                i *= 2;
            }
        }

        while (i <= bufLen) {
            int l = i;
            for (j = effStart; j + 2 * i < end; j += 2 * i) {
                mergeWithBuf(j, j + i, j + 2 * i, l);
            }
            if (j + i < end) {
                mergeWithBuf(j, j + i, end, l);
            } else {
                shift(j - l, j, end, false, ext);
            }

            i *= 2;

            for (j = effStart - l; j + 2 * i < end - l; j += 2 * i);

            if (j + i < end - l) {
                dualMergeBW(j, j + i, end - l, l);
            } else {
                shift(j, end - l, end, true, ext);
            }

            for (j -= 2 * i; j >= effStart - l; j -= 2 * i) {
                dualMergeBW(j, j + i, j + 2 * i, l);
            }

            i *= 2;
        }

        while (i < length) {
            for (j = effStart; j + 2 * i < end; j += 2 * i) {
                blockMerge(j, j + i, j + 2 * i, auxTag);
            }
            if (j + i < end) {
                blockMerge(j, j + i, end, auxTag);
            } else {
                shift(j - bufLen, j, end, false, ext);
            }

            i *= 2;
            if (i >= length) return true;

            for (j = start; j + 2 * i < end - bufLen; j += 2 * i);

            if (j + i < end - bufLen) {
                blockMergeBW(j, j + i, end - bufLen, auxTag);
            } else {
                shift(j, end - bufLen, end, true, ext);
            }

            for (j -= 2 * i; j >= start; j -= 2 * i) {
                blockMergeBW(j, j + i, j + 2 * i, auxTag);
            }

            i *= 2;
        }

        return false;
    }

public:
    Sorter(T* arr, Compare cmpFunc) : array(arr), comp(cmpFunc), ext(false) {}

    void sort(int start, int end) {
        int length = end - start;
        if (length <= 128) {
            inPlaceMergeSort2(start, end);
            return;
        }

        ext = false;
        bufPos = start;
        for (blockLen = 1; blockLen * blockLen < length; blockLen *= 2);

        int ideal = blockLen * 2;
        bufLen = findKeys(start, end, ideal);

        if (bufLen < ideal) {
            if (bufLen == 1) {
                return;
            } else {
                inPlaceMergeSort2(start, end);
                return;
            }
        }

        ideal = length / blockLen;
        tagLen = findKeys(start + bufLen, end, ideal);

        if (tagLen < ideal) {
            inPlaceMergeSort2(start, end);
            return;
        }

        int bufStart = start + tagLen;
        int effStart = bufStart + bufLen;
        int bufEnd = start + bufLen;
        shift(start, bufEnd, effStart, false, false);

        bool bw = kotaIterator(bufStart, end, false);

        if (bw) {
            int endStart = end - bufLen;
            multiSwap(start, endStart, tagLen, false);
            mergeWithBufStatic(start, bufStart, endStart, endStart, false);
            inPlaceMergeSort2(endStart, end);

            int mid = endStart + blockLen;
            int pos = binarySearch(start, endStart, array[mid - 1], true);
            rotate(pos, endStart, mid);
            pos += blockLen;

            multiSwapBW(end - 1, pos - 1, blockLen, false);
            mergeWithBufStatic(start, pos - blockLen, pos, mid, true);
            inPlaceMergeSort2(mid, end);
            inPlaceMergeBW(pos, mid, end);
        } else {
            mergeWithBufStatic(bufEnd, effStart, end, start, false);
            inPlaceMergeSort2(start, bufEnd);

            int mid = start + blockLen;
            int pos = binarySearch(bufEnd, end, array[mid], true);
            rotate(mid, bufEnd, pos);
            pos -= blockLen;

            multiSwap(start, pos, blockLen, false);
            mergeWithBufStatic(pos, pos + blockLen, end, start, false);
            inPlaceMergeSort2(start, mid);
            inPlaceMerge(start, mid, pos);
        }
    }
};

template <typename T, typename Compare>
void sort(T* array, size_t length, Compare comp) {
    Sorter<T, Compare> sorter(array, comp);
    sorter.sort(0, static_cast<int>(length));
}

template <typename T>
void sort(T* array, size_t length) {
    sort(array, length, std::less<T>());
}

} // namespace KotaClean

#endif // KOTA_SORT_CLEAN_HPP
