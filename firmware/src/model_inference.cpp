#include "model_inference.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

namespace {
std::vector<float> g_fft_mags;
}

void init_model_inference() {
    g_fft_mags.reserve(kMaxRfSamples);
}

// Simple O(N^2) DFT magnitude for embedded portability when no FFT lib is present.
static void compute_fft_mag(const RFSampleWindow& window, std::vector<float>& mags_out) {
    const std::size_t N = window.sample_count;
    mags_out.assign(N / 2 + 1, 0.0f);
    const float invN = 1.0f / static_cast<float>(N);

    for (std::size_t k = 0; k < mags_out.size(); ++k) {
        std::complex<float> acc{0.0f, 0.0f};
        for (std::size_t n = 0; n < N; ++n) {
            const float angle = -2.0f * 3.14159265f * static_cast<float>(k * n) * invN;
            const float sample = static_cast<float>(window.samples[n]);
            acc += std::complex<float>(sample * std::cos(angle), sample * std::sin(angle));
        }
        mags_out[k] = std::abs(acc) * invN;
    }
}

RfFeatures extract_rf_features(const RFSampleWindow& window) {
    RfFeatures features{};

    if (window.sample_count == 0) {
        return features;
    }

    compute_fft_mag(window, g_fft_mags);
    const auto max_it = std::max_element(g_fft_mags.begin(), g_fft_mags.end());
    const float peak = (max_it != g_fft_mags.end()) ? *max_it : 0.0f;
    const float avg = std::accumulate(g_fft_mags.begin(), g_fft_mags.end(), 0.0f) /
                      static_cast<float>(g_fft_mags.size());

    // Placeholder scaling to dBm-ish values for visualization.
    features.avg_dbm = 20.0f * std::log10(std::max(avg, 1e-6f)) - 30.0f;
    features.peak_dbm = 20.0f * std::log10(std::max(peak, 1e-6f)) - 20.0f;
    return features;
}

float run_model_inference(const RfFeatures& features) {
    // Toy anomaly score: normalized difference between peak and average
    const float delta = features.peak_dbm - features.avg_dbm;
    const float normalized = delta / 20.0f;
    if (normalized < 0.0f) {
        return 0.0f;
    }
    if (normalized > 1.0f) {
        return 1.0f;
    }
    return normalized;
}
