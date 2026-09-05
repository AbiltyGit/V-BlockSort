#pragma once

#include "cegar_types.hpp"
#include "sat_adversary.hpp"
#include <vector>
#include <functional>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace SatCegar {

struct SynthesisConfig {
    int N;
    int mid_split;
    int max_depth = 12;
    bool allow_rotations = true;
    bool allow_stable_swaps = true;
    bool allow_key_swaps = false;
};

class CegarLoop {
private:
    SynthesisConfig config;
    SatAdversary adversary;
    std::vector<CounterExample> cex_history;

    // Checks if two operations commute and can be ordered canonically to break search symmetry
    static bool are_independent(const Operation& op1, const Operation& op2) {
        if (op1.type != OpType::STABLE_SWAP && op1.type != OpType::COND_SWAP) return false;
        if (op2.type != OpType::STABLE_SWAP && op2.type != OpType::COND_SWAP) return false;
        return (op1.a != op2.a && op1.a != op2.b && op1.b != op2.a && op1.b != op2.b);
    }

public:
    CegarLoop(const SynthesisConfig& cfg) 
        : config(cfg), adversary(cfg.N, cfg.mid_split) {}

    // Generates the pool of candidate operations
    std::vector<Operation> generate_candidate_operations() const {
        std::vector<Operation> ops;
        int N = config.N;

        // 1. Stable pairwise conditional swaps across distinct runs / indices
        if (config.allow_stable_swaps) {
            for (int i = 0; i < N; ++i) {
                for (int j = i + 1; j < N; ++j) {
                    ops.push_back(Operation{OpType::STABLE_SWAP, i, j, 0, 0});
                }
            }
        }

        // 2. Block rotations: rotate(first, mid, last)
        if (config.allow_rotations) {
            for (int first = 0; first < N; ++first) {
                for (int mid = first + 1; mid < N; ++mid) {
                    for (int last = mid + 1; last <= N; ++last) {
                        ops.push_back(Operation{OpType::ROTATE, first, mid, last, 0});
                    }
                }
            }
        }

        return ops;
    }

    bool passes_all_known_cex(const Program& prog) const {
        for (const auto& cex : cex_history) {
            auto out = adversary.simulate(prog, cex.input);
            std::string err;
            if (!adversary.check_sorted_and_stable(out, err)) {
                return false;
            }
        }
        return true;
    }

    std::optional<Program> synthesize() {
        auto start_time = std::chrono::high_resolution_clock::now();
        auto candidate_ops = generate_candidate_operations();

        std::cout << "[CEGAR] Initialized synthesis for N=" << config.N 
                  << " (A=[0.." << config.mid_split - 1 
                  << "], B=[" << config.mid_split << ".." << config.N - 1 << "])" << std::endl;
        std::cout << "[CEGAR] Candidate operation pool size: " << candidate_ops.size() << std::endl;

        // Seed with initial failing counterexample
        Program empty_prog;
        auto initial_cex = adversary.find_counterexample_cadical(empty_prog);
        if (initial_cex) {
            cex_history.push_back(*initial_cex);
        }

        for (int depth = 1; depth <= config.max_depth; ++depth) {
            std::cout << "[CEGAR] Searching depth " << depth << " (Known CEX count: " << cex_history.size() << ")..." << std::endl;

            Program current_prog;
            bool found = false;

            std::function<bool(int, int)> dfs = [&](int cur_depth, int last_op_idx) -> bool {
                if (cur_depth == depth) {
                    if (!passes_all_known_cex(current_prog)) {
                        return false;
                    }

                    auto cex = adversary.find_counterexample_cadical(current_prog);
                    if (!cex) {
                        found = true;
                        return true;
                    }

                    cex_history.push_back(*cex);
                    return false;
                }

                for (size_t i = 0; i < candidate_ops.size(); ++i) {
                    const auto& op = candidate_ops[i];

                    // Symmetry Breaking: If current op and previous op are independent, enforce canonical index order
                    if (!current_prog.empty()) {
                        const auto& prev_op = current_prog.back();
                        if (are_independent(prev_op, op) && (int)i < last_op_idx) {
                            continue;
                        }
                        // Avoid consecutive identical operations
                        if (prev_op.type == op.type && prev_op.a == op.a && prev_op.b == op.b && prev_op.c == op.c) {
                            continue;
                        }
                    }

                    current_prog.push_back(op);
                    if (dfs(cur_depth + 1, (int)i)) return true;
                    current_prog.pop_back();
                }
                return false;
            };

            if (dfs(0, -1)) {
                auto end_time = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration<double, std::milli>(end_time - start_time).count();
                std::cout << "\n========================================\n";
                std::cout << ">>> OPTIMAL INVARIANT DISCOVERED! <<<\n";
                std::cout << "Depth: " << depth << " ops | CEX Refinements: " << cex_history.size() 
                          << " | Time: " << elapsed << " ms\n";
                std::cout << "========================================\n";
                for (size_t i = 0; i < current_prog.size(); ++i) {
                    std::cout << "  Step " << std::setw(2) << (i + 1) << ": " << current_prog[i].to_string() << "\n";
                }
                std::cout << "========================================\n\n";
                return current_prog;
            }
        }

        std::cout << "[CEGAR] No valid program found within depth bound " << config.max_depth << std::endl;
        return std::nullopt;
    }
};

} // namespace SatCegar
