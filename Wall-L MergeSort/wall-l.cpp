#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

// En dip katmanda (Wall-1) blokları sıralamak için Quadratic (kaaresel) sıralama
template <typename T>
void wall1InsertionSort(std::vector<T>& arr, int start, int end) {
    for (int i = start + 1; i < end; ++i) {
        T key = arr[i];
        int j = i - 1;
        while (j >= start && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// Standart iki yönlü kararlı birleştirme (Merge) yardımcı fonksiyonu
template <typename T>
void mergeSubarrays(std::vector<T>& arr, int low, int mid, int high) {
    int left_ptr = low;
    int right_ptr = mid;
    std::vector<T> buffer;
    buffer.reserve(high - low);

    while (left_ptr < mid && right_ptr < high) {
        if (arr[left_ptr] <= arr[right_ptr]) {
            buffer.push_back(arr[left_ptr]);
            left_ptr++;
        } else {
            buffer.push_back(arr[right_ptr]);
            right_ptr++;
        }
    }

    while (left_ptr < mid)  buffer.push_back(arr[left_ptr]);
    while (right_ptr < high) buffer.push_back(arr[right_ptr]);

    for (size_t i = 0; i < buffer.size(); ++i) {
        arr[low + i] = buffer[i];
    }
}

// Rekürsif Katmanlı Bölme ve Birleştirme Fonksiyonu
template <typename T>
void wallLMergeSortRecursive(std::vector<T>& arr, int start, int end, int current_L) {
    int length = end - start;
    if (length <= 1) return;

    // Eğer 1. katman duvarına (Wall-1) ulaştıysak, quadratic sort uygula
    if (current_L <= 1) {
        wall1InsertionSort(arr, start, end);
        return;
    }

    // Makale formülasyonuna uygun olarak, bir sonraki katmanın blok boyutunu (K) hesapla
    // L katmanına göre alt blok boyutunu belirleyen adım (Yaklaşık n^(1/L))
    int next_block_size = std::max(2, static_cast<int>(std::pow(length, 1.0 / current_L)));

    // Alt katmandaki (Wall - L-1) blokları rekürsif olarak çağır ve sırala
    for (int i = start; i < end; i += next_block_size) {
        int next_end = std::min(i + next_block_size, end);
        wallLMergeSortRecursive(arr, i, next_end, current_L - 1);
    }

    // Bu katmandaki (Wall-L) sıralı alt blokları yukarı doğru birleştir (Conquer)
    int merge_width = next_block_size;
    while (merge_width < length) {
        for (int i = start; i < end; i += 2 * merge_width) {
            int low = i;
            int mid = std::min(i + merge_width, end);
            int high = std::min(i + 2 * merge_width, end);

            if (mid < high && arr[mid - 1] > arr[mid]) {
                mergeSubarrays(arr, low, mid, high);
            }
        }
        merge_width *= 2;
    }
}

// Kullanıcı dostu sarmalayıcı (Wrapper) fonksiyonu
template <typename T>
void wallLMergeSort(std::vector<T>& arr, int L) {
    if (L < 1) L = 1; // Katman en az 1 olmalıdır (Sadece Insertion Sort olur)
    wallLMergeSortRecursive(arr, 0, arr.size(), L);
}

// Test ve Sürücü Kod
int main() {
    std::vector<int> data = {42, 12, 89, 23, 11, 6, 74, 55, 33, 90, 4, 15, 22, 61};

    // L = 3 katmanlı bir duvar birleştirmesi seçelim (Wall-3 Merge Sort)
    int layers = 3;

    std::cout << "Orijinal Dizi: ";
    for (int x : data) std::cout << x << " ";
    std::cout << "\n";

    wallLMergeSort(data, layers);

    std::cout << "Wall-" << layers << " Sonrasi: ";
    for (int x : data) std::cout << x << " ";
    std::cout << "\n";

    return 0;
}
