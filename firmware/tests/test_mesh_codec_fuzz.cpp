#include "mesh_encode.hpp"
#include "mesh.hpp"
#include "telemetry.hpp"
#include "mock_radio.hpp"

#include <cassert>
#include <random>
#include <cstring>

// Simple decode stub: for now we only validate encoding length and that encrypt returns non-zero length.
static bool sanity_check_encode(const MeshFrame& frame, EncryptedFrame& out) {
    AesGcmKey key{};
    key.bytes.fill(0x22);
    out = encrypt_mesh_frame(frame, key);
    return out.len > 0 && out.len <= kMaxCipherLen;
}

static MeshFrame make_frame(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist_byte(0, 255);
    MeshFrame f{};
    f.header.version = 1;
    f.header.msg_type = MeshMsgType::Telemetry;
    f.header.ttl = 4;
    f.header.hop_count = 0;
    f.header.seq_no = dist_byte(rng);
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "node-%d", dist_byte(rng) % 50);
    std::snprintf(f.header.dest_node_id, sizeof(f.header.dest_node_id), "gw");

    f.telemetry.rf_event.timestamp_ms = dist_byte(rng);
    f.telemetry.rf_event.center_freq_hz = 915000000;
    f.telemetry.rf_event.features.avg_dbm = -60.0f;
    f.telemetry.rf_event.features.peak_dbm = -40.0f;
    f.telemetry.rf_event.anomaly_score = 0.1f * static_cast<float>(dist_byte(rng) % 10);
    f.telemetry.rf_event.model_version = 1;

    f.telemetry.gps.timestamp_ms = 0;
    f.telemetry.gps.valid_fix = true;
    f.telemetry.health.timestamp_ms = 0;
    f.telemetry.health.battery_v = 3.7f;

    f.routing.entry_count = 0;
    f.fault.fault_active = false;
    f.ota.state = OtaState::Idle;

    return f;
}

int main() {
    std::mt19937 rng(123);
    MockRadio radio;
    int good = 0;
    for (int i = 0; i < 200; ++i) {
        MeshFrame frame = make_frame(rng);
        EncryptedFrame enc{};
        bool ok = sanity_check_encode(frame, enc);
        if (!ok) {
            return 1;
        }
        radio.transmit(enc);
        ++good;
    }

    assert(radio.sent_count() == static_cast<std::size_t>(good));
    return 0;
}
