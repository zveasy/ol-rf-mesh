#include "model_inference.hpp"
#include "telemetry.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    // Initialize inference state
    init_model_inference();

    // Build a simple RF sample window with a clear tone-like pattern.
    RFSampleWindow window{};
    window.timestamp_ms = 0;
    window.center_freq_hz = 915000000;
    window.sample_count = 32;  // small but >0

    for (std::size_t i = 0; i < window.sample_count; ++i) {
        // Simple increasing ramp in amplitude
        window.samples[i] = static_cast<int16_t>(i * 10);
    }

    const RfFeatures features = extract_rf_features(window);

    // Basic sanity: peak should be >= avg, and both in a reasonable dBm-ish range.
    assert(features.peak_dbm >= features.avg_dbm);
    assert(std::isfinite(features.avg_dbm));
    assert(std::isfinite(features.peak_dbm));

    const float score = run_model_inference(features);

    // Anomaly score is normalized to [0,1].
    assert(score >= 0.0f - 1e-5f);
    assert(score <= 1.0f + 1e-5f);

    std::cout << "OK test_model_inference: avg_dbm=" << features.avg_dbm
              << " peak_dbm=" << features.peak_dbm
              << " score=" << score << "\n";

    return 0;
}
