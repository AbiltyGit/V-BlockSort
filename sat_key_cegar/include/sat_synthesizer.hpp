#pragma once

#include "cegar_types.hpp"
#include "sat_adversary.hpp"
#include "../third_party/include/cadical.hpp"
#include <vector>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <cassert>

namespace SatCegar {

class DirectSatSynthesizer {
private:
    int N;
    int mid_split;
    SatAdversary adversary;

public:
    DirectSatSynthesizer(int n, int split) 
        : N(n), mid_split(split), adversary(n, split) {}

    std::optional<Program> synthesize_cegar(int min_depth, int max_depth) {
        auto start = std::chrono::high_resolution_clock::now();

        // 1. Generate all valid 0-1 test vectors under sorted preconditions
        std::vector<std::vector<int>> all_valid_01;
        for (int mask = 0; mask < (1 << N); ++mask) {
            std::vector<int> v(N);
            for (int i = 0; i < N; ++i) v[i] = (mask >> i) & 1;

            bool a_sorted = true;
            for (int i = 0; i < mid_split - 1; ++i) if (v[i] > v[i + 1]) { a_sorted = false; break; }
            if (!a_sorted) continue;

            bool b_sorted = true;
            for (int j = mid_split; j < N - 1; ++j) if (v[j] > v[j + 1]) { b_sorted = false; break; }
            if (!b_sorted) continue;

            all_valid_01.push_back(v);
        }

        int num_wires = N;
        int num_pairs = N * (N - 1) / 2;

        auto pair_to_ij = [&](int p) -> std::pair<int, int> {
            int cnt = 0;
            for (int i = 0; i < N; ++i) {
                for (int j = i + 1; j < N; ++j) {
                    if (cnt == p) return {i, j};
                    cnt++;
                }
            }
            return {0, 1};
        };

        for (int depth = min_depth; depth <= max_depth; ++depth) {
            int K = depth;
            std::cout << "[CEGAR] Searching Depth K = " << K << " with CaDiCaL & Stability Verifier..." << std::endl;

            CaDiCaL::Solver solver;

            auto var_sel = [&](int t, int p) -> int {
                return 1 + t * num_pairs + p;
            };

            int var_offset = 1 + K * num_pairs;
            int num_vecs = all_valid_01.size();

            auto var_wire = [&](int v, int t, int w) -> int {
                return var_offset + (v * (K + 1) + t) * num_wires + w;
            };

            // Exactly one comparator per step
            for (int t = 0; t < K; ++t) {
                for (int p = 0; p < num_pairs; ++p) solver.add(var_sel(t, p));
                solver.add(0);

                for (int p1 = 0; p1 < num_pairs; ++p1) {
                    for (int p2 = p1 + 1; p2 < num_pairs; ++p2) {
                        solver.add(-var_sel(t, p1));
                        solver.add(-var_sel(t, p2));
                        solver.add(0);
                    }
                }
            }

            // Wire semantics
            for (int v = 0; v < num_vecs; ++v) {
                for (int w = 0; w < N; ++w) {
                    if (all_valid_01[v][w] > 0) solver.add(var_wire(v, 0, w));
                    else solver.add(-var_wire(v, 0, w));
                    solver.add(0);
                }

                for (int t = 0; t < K; ++t) {
                    for (int p = 0; p < num_pairs; ++p) {
                        auto [i, j] = pair_to_ij(p);
                        int s = var_sel(t, p);

                        int wi_next = var_wire(v, t + 1, i);
                        int wj_next = var_wire(v, t + 1, j);
                        int wi_curr = var_wire(v, t, i);
                        int wj_curr = var_wire(v, t, j);

                        solver.add(-s); solver.add(-wi_next); solver.add(wi_curr); solver.add(0);
                        solver.add(-s); solver.add(-wi_next); solver.add(wj_curr); solver.add(0);
                        solver.add(-s); solver.add(wi_next); solver.add(-wi_curr); solver.add(-wj_curr); solver.add(0);

                        solver.add(-s); solver.add(wj_next); solver.add(-wi_curr); solver.add(0);
                        solver.add(-s); solver.add(wj_next); solver.add(-wj_curr); solver.add(0);
                        solver.add(-s); solver.add(-wj_next); solver.add(wi_curr); solver.add(wj_curr); solver.add(0);

                        for (int k = 0; k < N; ++k) {
                            if (k == i || k == j) continue;
                            int wk_next = var_wire(v, t + 1, k);
                            int wk_curr = var_wire(v, t, k);
                            solver.add(-s); solver.add(-wk_next); solver.add(wk_curr); solver.add(0);
                            solver.add(-s); solver.add(wk_next); solver.add(-wk_curr); solver.add(0);
                        }
                    }
                }

                // Sortedness check at step K
                for (int w = 0; w < N - 1; ++w) {
                    int curr = var_wire(v, K, w);
                    int next = var_wire(v, K, w + 1);
                    solver.add(-curr); solver.add(next); solver.add(0);
                }
            }

            int cegar_iterations = 0;
            while (true) {
                cegar_iterations++;
                int res = solver.solve();
                if (res != 10) {
                    std::cout << "  -> Depth " << K << " is UNSAT after " << cegar_iterations << " iterations.\n";
                    break;
                }

                // Extract candidate program
                Program candidate;
                std::vector<int> chosen_sel_vars;
                for (int t = 0; t < K; ++t) {
                    for (int p = 0; p < num_pairs; ++p) {
                        if (solver.val(var_sel(t, p)) > 0) {
                            auto [i, j] = pair_to_ij(p);
                            candidate.push_back(Operation{OpType::STABLE_SWAP, i, j, 0, 0});
                            chosen_sel_vars.push_back(var_sel(t, p));
                        }
                    }
                }

                // Verify with 3-valued strict stability adversary
                auto cex = adversary.find_counterexample_exhaustive(candidate);
                if (!cex) {
                    // SUCCESS! 100% stable and sorted invariant discovered!
                    auto end = std::chrono::high_resolution_clock::now();
                    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

                    std::cout << "\n=======================================================\n";
                    std::cout << ">>> 100% PROVABLY STABLE INVARIANT DISCOVERED! <<<\n";
                    std::cout << "Configuration: Merge " << mid_split << " + " << (N - mid_split) 
                              << " (Total N = " << N << ")\n";
                    std::cout << "Minimal Operation Count: " << K << " ops | CEGAR Iterations: " << cegar_iterations << "\n";
                    std::cout << "Synthesis Time: " << elapsed << " ms\n";
                    std::cout << "=======================================================\n";
                    for (size_t i = 0; i < candidate.size(); ++i) {
                        std::cout << "  Step " << std::setw(2) << (i + 1) << ": " << candidate[i].to_string() << "\n";
                    }
                    std::cout << "=======================================================\n\n";
                    return candidate;
                }

                // CEGAR Refinement: Add blocking clause forbidding this exact candidate sequence
                for (int v_sel : chosen_sel_vars) {
                    solver.add(-v_sel);
                }
                solver.add(0);
            }
        }

        return std::nullopt;
    }
};

} // namespace SatCegar
