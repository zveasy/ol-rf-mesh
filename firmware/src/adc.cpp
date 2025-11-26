#include "adc.hpp"

void init_adc() {
}

RFSampleWindow collect_rf_window(uint32_t now_ms) {
    RFSampleWindow window{};
    window.timestamp_ms = now_ms;
    window.center_freq_hz = 915000000; // placeholder ISM band
    window.sample_count = kMaxRfSamples;

    for (std::size_t i = 0; i < window.sample_count; ++i) {
        // Simple stub waveform: ramp + small spike
        window.samples[i] = static_cast<int16_t>(i % 64);
    }

    // Inject a peak to mimic a tone
    if (!window.samples.empty()) {
        window.samples[5] = 200;
    }

    return window;
}
