#include "config.hpp"

NodeConfig load_config() {
    NodeConfig cfg{};
    cfg.node_id = "node-001";
    cfg.report_interval_ms = 1000;
    cfg.anomaly_threshold = 0.8f;
    return cfg;
}
