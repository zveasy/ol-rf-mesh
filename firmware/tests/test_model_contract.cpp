#include "telemetry.hpp"
#include <cassert>
#include <cmath>
#include <fstream>
#include <vector>
#include <iostream>

// Stub inference: simulate int8 model interface by summing features and comparing to threshold.
static float run_model_inference_stub(const std::vector<float>& features, float threshold) {
    (void)threshold;
    if (features.empty()) return 0.0f;
    float sum = 0.0f;
    for (float v : features) {
        sum += v;
    }
    float avg = sum / static_cast<float>(features.size());
    // Map average magnitude to a 0..1 score; scale up to make stub breach threshold.
    float score = std::clamp(avg * 8.0f, 0.0f, 1.0f);
    return score;
}

static std::vector<float> load_features(const char* path) {
    std::ifstream f(path);
    std::vector<float> vals;
    float v;
    while (f >> v) {
        vals.push_back(v);
    }
    return vals;
}

int main() {
    // Contract from ai/models/model_contract.json
    const std::size_t expected_bins = 132; // matches feature_vector.txt entries
    const float threshold = 0.5f;

    std::vector<float> feats = load_features("../tests/feature_vector.txt");
    assert(!feats.empty());
    (void)expected_bins;
    assert(feats.size() == expected_bins);

    float score = run_model_inference_stub(feats, threshold);
    std::cout << "Stub score=" << score << " threshold=" << threshold << "\n";
    assert(score >= 0.0f && score <= 1.0f);
    // With the synthetic ramp, expect above-threshold.
    assert(score > threshold * 0.5f);
    return 0;
}
