#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cstdint>
#include <sstream>
#include <algorithm>

namespace SatCegar {

enum class OpType {
    COND_SWAP,      // if (A[j] < A[i]) swap(A[i], A[j])
    STABLE_SWAP,    // if (A[j] < A[i] || (A[j] == A[i] && orig[j] < orig[i])) swap(A[i], A[j])
    ROTATE,         // std::rotate(first, mid, last)
    KEY_COND_SWAP,  // if (key_cmp(k1, k2)) swap_blocks(...)
    BLOCK_SWAP      // swap(A[a..a+len], A[b..b+len])
};

struct Operation {
    OpType type;
    int a;
    int b;
    int c; // For rotate: mid or len
    int key_idx;

    std::string to_string() const {
        std::ostringstream oss;
        switch (type) {
            case OpType::COND_SWAP:
                oss << "CondSwap(" << a << ", " << b << ")";
                break;
            case OpType::STABLE_SWAP:
                oss << "StableCondSwap(" << a << ", " << b << ")";
                break;
            case OpType::ROTATE:
                oss << "Rotate(" << a << ", " << b << ", " << c << ")";
                break;
            case OpType::KEY_COND_SWAP:
                oss << "KeyCondSwap(blk1=" << a << ", blk2=" << b << ", len=" << c << ", key=" << key_idx << ")";
                break;
            case OpType::BLOCK_SWAP:
                oss << "BlockSwap(" << a << ", " << b << ", len=" << c << ")";
                break;
        }
        return oss.str();
    }
};

using Program = std::vector<Operation>;

// Represents a concrete element tracking both its value and original index for stability
struct TaggedElement {
    int value;
    int original_pos;

    bool operator<(const TaggedElement& other) const {
        if (value != other.value) return value < other.value;
        return original_pos < other.original_pos;
    }

    bool is_strictly_less(const TaggedElement& other) const {
        return value < other.value;
    }

    bool is_equal_val(const TaggedElement& other) const {
        return value == other.value;
    }
};

struct CounterExample {
    int N;
    std::vector<TaggedElement> input;
    std::string failure_reason;
};

} // namespace SatCegar
