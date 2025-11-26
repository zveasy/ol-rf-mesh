#pragma once

#include <string>
#include <cstdint>

struct NodeConfig {
    std::string node_id;
    uint32_t report_interval_ms;
    float anomaly_threshold;
};

NodeConfig load_config();
