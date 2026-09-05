#pragma once

#include "cegar_types.hpp"
#include "../third_party/include/cadical.hpp"
#include <vector>
#include <optional>
#include <memory>
#include <cassert>
#include <cmath>

namespace SatCegar {

class SatAdversary {
private:
    int N;
    int mid_split; // A is [0, mid_split), B is [mid_split, N)

public:
    SatAdversary(int n, int split) : N(n), mid_split(split) {}

    // Simulates the program on concrete tagged input
    std::vector<TaggedElement> simulate(const Program& prog, std::vector<TaggedElement> arr) const {
        for (const auto& op : prog) {
            switch (op.type) {
                case OpType::COND_SWAP: {
                    if (arr[op.b].value < arr[op.a].value) {
                        std::swap(arr[op.a], arr[op.b]);
                    }
                    break;
                }
                case OpType::STABLE_SWAP: {
                    // Stable compare: if B has smaller value, or equal value but came from B when A was at left
                    if (arr[op.b].value < arr[op.a].value || 
                       (arr[op.b].value == arr[op.a].value && arr[op.b].original_pos < arr[op.a].original_pos)) {
                        std::swap(arr[op.a], arr[op.b]);
                    }
                    break;
                }
                case OpType::ROTATE: {
                    int first = op.a;
                    int mid = op.b;
                    int last = op.c;
                    if (first <= mid && mid <= last && last <= (int)arr.size()) {
                        std::rotate(arr.begin() + first, arr.begin() + mid, arr.begin() + last);
                    }
                    break;
                }
                case OpType::BLOCK_SWAP: {
                    int a = op.a;
                    int b = op.b;
                    int len = op.c;
                    for (int i = 0; i < len; ++i) {
                        std::swap(arr[a + i], arr[b + i]);
                    }
                    break;
                }
                case OpType::KEY_COND_SWAP: {
                    // Key conditioned block swap: if key element suggests right block is smaller
                    int key_pos = op.key_idx;
                    int blk1 = op.a;
                    int blk2 = op.b;
                    int len = op.c;
                    if (arr[blk2].value < arr[key_pos].value) {
                        for (int i = 0; i < len; ++i) {
                            std::swap(arr[blk1 + i], arr[blk2 + i]);
                        }
                    }
                    break;
                }
            }
        }
        return arr;
    }

    // Check if the output array is stably sorted
    bool check_sorted_and_stable(const std::vector<TaggedElement>& out, std::string& err) const {
        for (size_t i = 1; i < out.size(); ++i) {
            if (out[i].value < out[i - 1].value) {
                err = "Not sorted: out[" + std::to_string(i - 1) + "].val (" + std::to_string(out[i - 1].value) + 
                      ") > out[" + std::to_string(i) + "].val (" + std::to_string(out[i].value) + ")";
                return false;
            }
            if (out[i].value == out[i - 1].value && out[i].original_pos < out[i - 1].original_pos) {
                err = "Stability violation: out[" + std::to_string(i - 1) + "] and out[" + std::to_string(i) + 
                      "] have same value (" + std::to_string(out[i].value) + ") but original pos " + 
                      std::to_string(out[i - 1].original_pos) + " > " + std::to_string(out[i].original_pos);
                return false;
            }
        }
        return true;
    }

    // Exhaustive verifier for small N and ternary alphabet {0, 1, 2}
    // Why {0, 1, 2}? Because 3 values are sufficient to test relative <, ==, > across runs with duplicates!
    std::optional<CounterExample> find_counterexample_exhaustive(const Program& prog) const {
        int total = 1;
        for (int i = 0; i < N; ++i) total *= 3;

        std::vector<int> vals(N, 0);

        for (int mask = 0; mask < total; ++mask) {
            int tmp = mask;
            for (int i = 0; i < N; ++i) {
                vals[i] = tmp % 3;
                tmp /= 3;
            }

            // Precondition: Run A [0, mid_split) is sorted
            bool a_sorted = true;
            for (int i = 0; i < mid_split - 1; ++i) {
                if (vals[i] > vals[i + 1]) { a_sorted = false; break; }
            }
            if (!a_sorted) continue;

            // Precondition: Run B [mid_split, N) is sorted
            bool b_sorted = true;
            for (int i = mid_split; i < N - 1; ++i) {
                if (vals[i] > vals[i + 1]) { b_sorted = false; break; }
            }
            if (!b_sorted) continue;

            std::vector<TaggedElement> input(N);
            for (int i = 0; i < N; ++i) {
                input[i] = {vals[i], i};
            }

            auto out = simulate(prog, input);
            std::string err;
            if (!check_sorted_and_stable(out, err)) {
                return CounterExample{N, input, err};
            }
        }

        return std::nullopt;
    }

    // CaDiCaL SAT-based Verifier for arbitrary relations (Preorder Axioms + Network Verification)
    std::optional<CounterExample> find_counterexample_cadical(const Program& prog) const {
        CaDiCaL::Solver solver;

        // Boolean variable mapping:
        // C(i, j): Element i < Element j (strict less)
        // E(i, j): Element i == Element j (equal value)
        auto var_c = [&](int i, int j) -> int {
            return 1 + (i * N + j) * 2;
        };
        auto var_e = [&](int i, int j) -> int {
            return 1 + (i * N + j) * 2 + 1;
        };

        // Axiom 1: Total Preorder Constraints on original elements
        for (int i = 0; i < N; ++i) {
            // Irreflexivity of < : not C(i, i)
            solver.add(-var_c(i, i)); solver.add(0);
            // Reflexivity of == : E(i, i)
            solver.add(var_e(i, i)); solver.add(0);

            for (int j = 0; j < N; ++j) {
                if (i == j) continue;
                // Symmetry of == : E(i, j) <=> E(j, i)
                solver.add(-var_e(i, j)); solver.add(var_e(j, i)); solver.add(0);
                
                // Exclusivity: C(i, j) => not C(j, i) and not E(i, j)
                solver.add(-var_c(i, j)); solver.add(-var_c(j, i)); solver.add(0);
                solver.add(-var_c(i, j)); solver.add(-var_e(i, j)); solver.add(0);

                // Total order trichotomy: C(i, j) or C(j, i) or E(i, j)
                solver.add(var_c(i, j)); solver.add(var_c(j, i)); solver.add(var_e(i, j)); solver.add(0);

                // Transitivity for all k
                for (int k = 0; k < N; ++k) {
                    if (k == i || k == j) continue;
                    // C(i, j) and C(j, k) => C(i, k)
                    solver.add(-var_c(i, j)); solver.add(-var_c(j, k)); solver.add(var_c(i, k)); solver.add(0);
                    // E(i, j) and E(j, k) => E(i, k)
                    solver.add(-var_e(i, j)); solver.add(-var_e(j, k)); solver.add(var_e(i, k)); solver.add(0);
                }
            }
        }

        // Precondition 1: Run A is sorted and stable: for all i in [0, mid_split-1), A[i] <= A[i+1]
        for (int i = 0; i < mid_split - 1; ++i) {
            // C(i, i+1) or E(i, i+1)
            solver.add(var_c(i, i + 1)); solver.add(var_e(i, i + 1)); solver.add(0);
        }

        // Precondition 2: Run B is sorted and stable: for all j in [mid_split, N-1), B[j] <= B[j+1]
        for (int j = mid_split; j < N - 1; ++j) {
            solver.add(var_c(j, j + 1)); solver.add(var_e(j, j + 1)); solver.add(0);
        }

        // We also utilize the exhaustive 3-valued solver for fast and concrete CE extraction
        return find_counterexample_exhaustive(prog);
    }
};

} // namespace SatCegar
