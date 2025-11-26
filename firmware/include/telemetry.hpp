#pragma once

#include <array>
#include <cstdint>
#include "fault.hpp"
#include "ota.hpp"

constexpr std::size_t kMaxNodeIdLength = 16;
constexpr std::size_t kMaxRfSamples = 128;
constexpr std::size_t kMaxRoutes = 8;
constexpr std::size_t kNonceLength = 12;
constexpr std::size_t kAuthTagLength = 16;

enum class MeshMsgType : uint8_t {
    Telemetry = 1,
    Routing = 2,
    Control = 3,
    Ota = 4,
};

struct RFSampleWindow {
    uint32_t timestamp_ms;
    uint32_t center_freq_hz;
    std::array<int16_t, kMaxRfSamples> samples;
    std::size_t sample_count;
};

struct RfFeatures {
    float avg_dbm;
    float peak_dbm;
};

struct RFEvent {
    uint32_t timestamp_ms;
    uint32_t center_freq_hz;
    RfFeatures features;
    float anomaly_score;
    uint8_t model_version;
};

struct GpsStatus {
    uint32_t timestamp_ms;
    float latitude_deg;
    float longitude_deg;
    float altitude_m;
    uint8_t num_sats;
    float hdop;
    bool valid_fix;
    bool jamming_detected;
    bool spoof_detected;
    float cn0_db_hz_avg;
};

struct HealthStatus {
    uint32_t timestamp_ms;
    float battery_v;
    float temp_c;
    float imu_tilt_deg;
    bool tamper_flag;
};

struct MeshFrameHeader {
    uint8_t version;
    MeshMsgType msg_type;
    uint8_t ttl;
    uint8_t hop_count;
    uint32_t seq_no;
    char src_node_id[kMaxNodeIdLength];
    char dest_node_id[kMaxNodeIdLength];
};

struct MeshSecurity {
    bool encrypted;
    std::array<uint8_t, kNonceLength> nonce;
    std::array<uint8_t, kAuthTagLength> auth_tag;
};

struct RouteEntry {
    char neighbor_id[kMaxNodeIdLength];
    int8_t rssi_dbm;
    uint8_t link_quality;
    uint8_t cost;
};

struct MeshRoutingPayload {
    uint32_t epoch_ms;
    std::array<RouteEntry, kMaxRoutes> entries;
    std::size_t entry_count;
};

struct MeshTelemetryPayload {
    RFEvent rf_event;
    GpsStatus gps;
    HealthStatus health;
};

struct MeshFrame {
    MeshFrameHeader header;
    MeshSecurity security;
    MeshTelemetryPayload telemetry;
    MeshRoutingPayload routing;
    FaultStatus fault;
    OtaStatus ota;
};

struct TaskHeartbeat {
    const char* name;
    uint32_t last_beat_ms;
};
