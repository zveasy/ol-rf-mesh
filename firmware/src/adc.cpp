#include "adc.hpp"
#include <algorithm>

#ifdef ESP_PLATFORM
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#endif

namespace {
#ifdef ESP_PLATFORM
adc_oneshot_unit_handle_t g_adc_unit = nullptr;
constexpr adc_unit_t kAdcUnit = ADC_UNIT_1;
constexpr adc_channel_t kAdcChannel = ADC_CHANNEL_0; // adjust per board (GPIO36 on ESP32-class)
bool g_adc_ready = false;

bool check_esp(esp_err_t err, const char* msg) {
    if (err != ESP_OK) {
        ESP_LOGE("ADC", "%s failed: %s", msg, esp_err_to_name(err));
        return false;
    }
    return true;
}
#endif
} // namespace

void init_adc() {
#ifdef ESP_PLATFORM
    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = kAdcUnit;
    unit_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;

    if (!check_esp(adc_oneshot_new_unit(&unit_cfg, &g_adc_unit), "adc_oneshot_new_unit")) {
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    chan_cfg.atten = ADC_ATTEN_DB_12;
    if (!check_esp(adc_oneshot_config_channel(g_adc_unit, kAdcChannel, &chan_cfg), "adc_oneshot_config_channel")) {
        return;
    }

    g_adc_ready = true;
    ESP_LOGI("ADC", "ADC oneshot ready (unit=%d channel=%d)", static_cast<int>(kAdcUnit), static_cast<int>(kAdcChannel));
#endif
}

RFSampleWindow collect_rf_window(uint32_t now_ms) {
    RFSampleWindow window{};
    window.timestamp_ms = now_ms;
    window.center_freq_hz = 915000000; // placeholder ISM band
    window.sample_count = kMaxRfSamples;

#ifdef ESP_PLATFORM
    if (!g_adc_ready) {
        ESP_LOGW("ADC", "collect_rf_window: ADC not initialized, returning zeroes");
        window.sample_count = 0;
        return window;
    }

    for (std::size_t i = 0; i < window.sample_count; ++i) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(g_adc_unit, kAdcChannel, &raw);
        if (err != ESP_OK) {
            ESP_LOGW("ADC", "adc_oneshot_read failed at idx=%zu: %s", i, esp_err_to_name(err));
            window.sample_count = i;
            break;
        }
        // Center 12-bit unsigned ADC to signed for FFT input.
        raw = std::clamp(raw, 0, 4095);
        window.samples[i] = static_cast<int16_t>(raw - 2048);
    }
#else
    for (std::size_t i = 0; i < window.sample_count; ++i) {
        // Simple stub waveform: ramp + small spike
        window.samples[i] = static_cast<int16_t>(i % 64);
    }
    if (!window.samples.empty()) {
        window.samples[5] = 200;
    }
#endif

    return window;
}
