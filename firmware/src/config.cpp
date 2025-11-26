#include "config.hpp"

NodeConfig load_config() {
    NodeConfig cfg{};
    cfg.node_id = "node-001";
    cfg.report_interval_ms = 1000;
    cfg.rf_center_freq_hz = 915000000;
    cfg.fft_size = 128;
    cfg.anomaly_threshold = 0.8f;
    cfg.heartbeat_interval_ms = 10000;
    cfg.mesh_key.fill(0x11);
    return cfg;
}
