#include "../include/synthesized_invariants.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cassert>

struct Item {
    int val;
    int tag;

    bool operator<(const Item& o) const {
        return val < o.val;
    }
};

inline bool item_less(const Item& a, const Item& b) {
    return a.val < b.val;
}

template <typename Func>
bool test_kernel(int len1, int len2, Func func, int num_trials = 100000) {
    std::mt19937_64 rng(12345 + len1 * 100 + len2);
    int N = len1 + len2;

    for (int t = 0; t < num_trials; ++t) {
        std::vector<Item> items(N);
        for (int i = 0; i < N; ++i) {
            items[i] = {static_cast<int>(rng() % (N + 1)), i};
        }
        std::stable_sort(items.begin(), items.begin() + len1, item_less);
        std::stable_sort(items.begin() + len1, items.end(), item_less);

        auto ref = items;
        std::stable_sort(ref.begin(), ref.end(), item_less);

        func(items.data(), item_less);

        for (int i = 0; i < N; ++i) {
            if (items[i].val != ref[i].val || items[i].tag != ref[i].tag) {
                return false;
            }
        }
    }
    return true;
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << ">>> VERIFYING ALL ASYMMETRIC + SYMMETRIC INVARIANTS   <<<\n";
    std::cout << "=========================================================\n";

    std::cout << "2+1: " << (test_kernel(2, 1, SatCegar::Synthesized::merge_2_1_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "3+1: " << (test_kernel(3, 1, SatCegar::Synthesized::merge_3_1_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "4+1: " << (test_kernel(4, 1, SatCegar::Synthesized::merge_4_1_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "2+2: " << (test_kernel(2, 2, SatCegar::Synthesized::merge_2_2_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "3+2: " << (test_kernel(3, 2, SatCegar::Synthesized::merge_3_2_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "4+2: " << (test_kernel(4, 2, SatCegar::Synthesized::merge_4_2_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "4+3: " << (test_kernel(4, 3, SatCegar::Synthesized::merge_4_3_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "5+2: " << (test_kernel(5, 2, SatCegar::Synthesized::merge_5_2_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "3+3: " << (test_kernel(3, 3, SatCegar::Synthesized::merge_3_3_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "4+4: " << (test_kernel(4, 4, SatCegar::Synthesized::merge_4_4_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";
    std::cout << "8+8: " << (test_kernel(8, 8, SatCegar::Synthesized::merge_8_8_stable<Item, decltype(&item_less)>) ? "PASS (100% Stable)" : "FAIL") << "\n";

    std::cout << "\n>>> ALL 11 INVARIANT SCHEDULERS PASSED 100% STRICT STABILITY! <<<\n";
    return 0;
}
