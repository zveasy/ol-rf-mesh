#include <cstdio>
#include "config.hpp"
#include "logging.hpp"
#include "spi_bus.hpp"
#include "adc.hpp"
#include "sensors.hpp"
#include "mesh.hpp"
#include "model_inference.hpp"
#include "tasks.hpp"
#include "ota.hpp"
#include "fault.hpp"
#include "mesh_packet.h"

static void delay_ms(unsigned int /*ms*/) {
    // Simple stub for now
}

int main() {
    init_logging();
    log_info("O&L RF Node booting...");

    NodeConfig cfg = load_config();

    init_spi_bus();
    init_adc();
    init_sensors();
    init_mesh();
    init_model_inference();
    init_ota();
    init_fault_monitor();

    RouteEntry gw{};
    std::snprintf(gw.neighbor_id, sizeof(gw.neighbor_id), "gateway");
    gw.rssi_dbm = -55;
    gw.link_quality = 90;
    gw.cost = 1;
    add_route_entry(gw);

    uint32_t now_ms = 0;
    for (int i = 0; i < 2; ++i) {
        TaskStatus status = run_firmware_cycle(cfg, now_ms);

        // Emit a raw legacy frame for the gateway decoder.
        rf_mesh_legacy_frame_t legacy{};
        legacy.header.version = 1;
        legacy.header.msg_type = 1;
        legacy.header.ttl = 4;
        legacy.header.hop_count = 0;
        legacy.header.seq_no = i + 1;
        std::snprintf(legacy.header.src_node_id, sizeof(legacy.header.src_node_id), "%s", cfg.node_id.c_str());
        legacy.header.dest_node_id[0] = '\0';

        legacy.telemetry.timestamp_ms = now_ms;
        legacy.telemetry.rf_power_dbm = status.packet_builder.last_beat_ms; // placeholder
        legacy.telemetry.battery_v = 3.7f;
        legacy.telemetry.temp_c = 25.0f;
        legacy.telemetry.anomaly_score = 0.42f;

        // RF event frame
        rf_mesh_rf_event_frame_t rf_evt{};
        rf_evt.header = legacy.header;
        rf_evt.header.msg_type = 2;
        rf_evt.payload.timestamp_ms = now_ms;
        rf_evt.payload.center_freq_hz = cfg.rf_center_freq_hz;
        rf_evt.payload.bin_width_hz = 12500.0f;
        rf_evt.payload.anomaly_score = 0.91f;
        rf_evt.payload.features[0] = -50.0f;
        rf_evt.payload.features[1] = -48.0f;
        rf_evt.payload.features[2] = -52.0f;
        rf_evt.payload.features[3] = -55.0f;
        std::snprintf(rf_evt.payload.band_id, sizeof(rf_evt.payload.band_id), "ISM");

        // GPS frame
        rf_mesh_gps_frame_t gps{};
        gps.header = legacy.header;
        gps.header.msg_type = 3;
        gps.payload.timestamp_ms = now_ms;
        gps.payload.num_sats = 8;
        gps.payload.snr_avg = 38.0f;
        gps.payload.hdop = 1.2f;
        gps.payload.valid_fix = 1;
        gps.payload.jamming_indicator = 0.0f;
        gps.payload.spoof_indicator = 0.0f;

        // Persist frames for gateway decoder tests.
        auto write_bin = [](const char* path, const void* data, size_t len) {
            FILE* f = std::fopen(path, "wb");
            if (f) {
                std::fwrite(data, len, 1, f);
                std::fclose(f);
            }
        };
        write_bin("legacy_frame.bin", &legacy, sizeof(legacy));
        write_bin("rf_event_frame.bin", &rf_evt, sizeof(rf_evt));
        write_bin("gps_frame.bin", &gps, sizeof(gps));

        std::printf(
            "\n[HEARTBEAT] rf=%u fft=%u gps=%u health=%u ota=%u fault=%u pkt=%u fault_active=%d\n",
            status.rf_scan.last_beat_ms,
            status.fft.last_beat_ms,
            status.gnss.last_beat_ms,
            status.health.last_beat_ms,
            status.ota.last_beat_ms,
            status.fault_monitor.last_beat_ms,
            status.packet_builder.last_beat_ms,
            status.faults.fault_active ? 1 : 0
        );

        delay_ms(cfg.report_interval_ms);
        now_ms += cfg.report_interval_ms;
    }

    return 0;
}
