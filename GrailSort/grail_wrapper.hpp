#ifndef GRAIL_WRAPPER_HPP
#define GRAIL_WRAPPER_HPP

#include <cstdint>
#include <cstddef>

struct BenchElement {
    int64_t key;
    uint32_t id;

    bool operator<(const BenchElement& other) const {
        return key < other.key;
    }

    bool operator<=(const BenchElement& other) const {
        return key <= other.key;
    }

    bool operator>(const BenchElement& other) const {
        return key > other.key;
    }

    bool operator==(const BenchElement& other) const {
        return key == other.key;
    }
};

namespace GrailWrapper {

inline int grail_elem_cmp(const BenchElement* a, const BenchElement* b) {
    if (a->key < b->key) return -1;
    if (a->key > b->key) return 1;
    return 0;
}

} // namespace GrailWrapper

#define SORT_TYPE BenchElement
#define SORT_CMP GrailWrapper::grail_elem_cmp
#include "GrailSort.h"
#undef SORT_TYPE
#undef SORT_CMP

namespace Grail {

inline void sort_in_place(BenchElement* arr, size_t len) {
    GrailSort(arr, static_cast<int>(len));
}

inline void sort_static_buffer(BenchElement* arr, size_t len) {
    GrailSortWithBuffer(arr, static_cast<int>(len));
}

inline void sort_dyn_buffer(BenchElement* arr, size_t len) {
    GrailSortWithDynBuffer(arr, static_cast<int>(len));
}

inline void sort_rec_stable(BenchElement* arr, size_t len) {
    RecStableSort(arr, static_cast<int>(len));
}

} // namespace Grail

#endif // GRAIL_WRAPPER_HPP
