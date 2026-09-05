#include "../include/sat_synthesizer.hpp"
#include <iostream>
#include <vector>

void synthesize_config(int len1, int len2, int max_depth) {
    int N = len1 + len2;
    std::cout << ">>> Synthesizing Asymmetric " << len1 << "+" << len2 << " Merge (N=" << N << ") <<<\n";
    SatCegar::DirectSatSynthesizer synth(N, len1);
    synth.synthesize_cegar(1, max_depth);
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << "   ASYMMETRIC IN-PLACE STABLE MERGE SYNTHESIS (CaDiCaL)  \n";
    std::cout << "=========================================================\n\n";

    synthesize_config(2, 1, 4);
    synthesize_config(3, 1, 5);
    synthesize_config(4, 1, 6);
    synthesize_config(4, 2, 7);
    synthesize_config(4, 3, 9);
    synthesize_config(5, 2, 8);
    synthesize_config(5, 3, 10);
    synthesize_config(6, 2, 9);

    return 0;
}
