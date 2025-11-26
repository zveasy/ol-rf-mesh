#pragma once

#include <stdint.h>

// Packed header for raw mesh telemetry frames (legacy).
typedef struct __attribute__((packed)) {
    uint8_t version;     // 1
    uint8_t msg_type;    // 1=telemetry
    uint8_t ttl;
    uint8_t hop_count;
    uint32_t seq_no;
    char src_node_id[16];
    char dest_node_id[16];
} rf_mesh_header_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;
    float rf_power_dbm;
    float battery_v;
    float temp_c;
    float anomaly_score;
} rf_mesh_telemetry_legacy_t;

typedef struct __attribute__((packed)) {
    rf_mesh_header_t header;
    rf_mesh_telemetry_legacy_t telemetry;
} rf_mesh_legacy_frame_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;
    uint32_t center_freq_hz;
    float bin_width_hz;
    float anomaly_score;
    float features[4]; // small fixed feature vector
    char band_id[8];
} rf_mesh_rf_event_t;

typedef struct __attribute__((packed)) {
    rf_mesh_header_t header;
    rf_mesh_rf_event_t payload;
} rf_mesh_rf_event_frame_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;
    uint8_t num_sats;
    float snr_avg;
    float hdop;
    uint8_t valid_fix;
    float jamming_indicator;
    float spoof_indicator;
} rf_mesh_gps_t;

typedef struct __attribute__((packed)) {
    rf_mesh_header_t header;
    rf_mesh_gps_t payload;
} rf_mesh_gps_frame_t;
