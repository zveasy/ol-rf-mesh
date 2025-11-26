#pragma once

#include <string>
#include <cstdint>
#include <array>

struct NodeConfig {
    std::string node_id;
    uint32_t report_interval_ms;
    uint32_t rf_center_freq_hz;
    uint16_t fft_size;
    float anomaly_threshold;
    uint32_t heartbeat_interval_ms;
    std::array<uint8_t, 32> mesh_key;
};

NodeConfig load_config();
