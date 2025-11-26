#include "tasks.hpp"
#include "adc.hpp"
#include "mesh.hpp"
#include "model_inference.hpp"
#include "sensors.hpp"
#include "ota.hpp"
#include "fault.hpp"
#include <cstdio>
#include <cstring>

namespace {
struct TaskQueues {
    RFSampleWindow last_rf_window{};
    RFEvent last_rf_event{};
    GpsStatus last_gps{};
    HealthStatus last_health{};
};

TaskQueues g_queues{};
uint32_t g_seq_no = 0;

void touch(TaskHeartbeat& hb, uint32_t now_ms) {
    hb.last_beat_ms = now_ms;
}

void rf_scan_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    g_queues.last_rf_window = collect_rf_window(now_ms);
    g_queues.last_rf_window.center_freq_hz = cfg.rf_center_freq_hz;
    touch(hb, now_ms);
}

void fft_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    const RfFeatures features = extract_rf_features(g_queues.last_rf_window);
    const float score = run_model_inference(features);

    g_queues.last_rf_event.timestamp_ms = now_ms;
    g_queues.last_rf_event.center_freq_hz = cfg.rf_center_freq_hz;
    g_queues.last_rf_event.features = features;
    g_queues.last_rf_event.anomaly_score = score;
    g_queues.last_rf_event.model_version = 1;

    touch(hb, now_ms);
}

void gnss_task(uint32_t now_ms, TaskHeartbeat& hb) {
    g_queues.last_gps = read_gps_status(now_ms);
    touch(hb, now_ms);
}

void health_task(uint32_t now_ms, TaskHeartbeat& hb) {
    g_queues.last_health = read_health_status(now_ms);
    touch(hb, now_ms);
}

void packet_builder_task(const NodeConfig& cfg, uint32_t now_ms, TaskHeartbeat& hb) {
    MeshFrame frame{};
    frame.header.version = 1;
    frame.header.msg_type = MeshMsgType::Telemetry;
    frame.header.ttl = 4;
    frame.header.hop_count = 0;
    frame.header.seq_no = ++g_seq_no;

    std::snprintf(frame.header.src_node_id, sizeof(frame.header.src_node_id), "%s", cfg.node_id.c_str());
    frame.header.dest_node_id[0] = '\0'; // broadcast in stub

    frame.security.encrypted = true;
    for (std::size_t i = 0; i < frame.security.nonce.size(); ++i) {
        frame.security.nonce[i] = static_cast<uint8_t>((g_seq_no >> (i % 4)) & 0xFF);
    }
    frame.security.auth_tag.fill(0); // placeholder until AES-GCM integrated

    frame.telemetry.rf_event = g_queues.last_rf_event;
    frame.telemetry.gps = g_queues.last_gps;
    frame.telemetry.health = g_queues.last_health;

    frame.routing = current_routing_payload();
    frame.routing.epoch_ms = now_ms;
    frame.fault = fault_status();
    frame.ota = ota_status();

    send_mesh_frame(frame);
    touch(hb, now_ms);
}

void ota_task(uint32_t now_ms, TaskHeartbeat& hb) {
    // Stub: pretend OTA is idle unless a failure was recorded
    (void)now_ms;
    touch(hb, now_ms);
}

void fault_task(uint32_t now_ms, TaskHeartbeat& hb) {
    // If tamper flag set, record a tamper fault
    if (g_queues.last_health.tamper_flag) {
        record_tamper();
    }
    touch(hb, now_ms);
}
} // namespace

TaskStatus run_firmware_cycle(const NodeConfig& cfg, uint32_t now_ms) {
    static TaskStatus status{
        {"RFScanTask", 0},
        {"FFTTflmTask", 0},
        {"GNSSMonitorTask", 0},
        {"SensorHealthTask", 0},
        {"PacketBuilderTask", 0},
        {"OtaUpdateTask", 0},
        {"FaultMonitorTask", 0},
        {},
    };

    rf_scan_task(cfg, now_ms, status.rf_scan);
    fft_task(cfg, now_ms, status.fft);
    gnss_task(now_ms, status.gnss);
    health_task(now_ms, status.health);
    ota_task(now_ms, status.ota);
    fault_task(now_ms, status.fault_monitor);
    packet_builder_task(cfg, now_ms, status.packet_builder);

    return status;
}
