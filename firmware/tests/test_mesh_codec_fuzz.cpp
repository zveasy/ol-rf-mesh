#include "mesh_encode.hpp"
#include "mesh.hpp"
#include "telemetry.hpp"
#include "mock_radio.hpp"

#include <cassert>
#include <random>
#include <cstring>

static MeshFrame make_frame(std::mt19937& rng, uint32_t seq) {
    std::uniform_int_distribution<int> dist_byte(0, 255);
    MeshFrame f{};
    f.header.version = 1;
    f.header.msg_type = MeshMsgType::Telemetry;
    f.header.ttl = 4;
    f.header.hop_count = 0;
    f.header.seq_no = seq;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "node-fuzz");
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
    AesGcmKey key{};
    key.bytes.fill(0x22);
    int good = 0;
    for (int i = 0; i < 200; ++i) {
        MeshFrame frame = make_frame(rng, static_cast<uint32_t>(i + 1));
        EncryptedFrame enc = encrypt_mesh_frame(frame, key);
        if (enc.len == 0 || enc.len > kMaxCipherLen) {
            return 1;
        }
        radio.transmit(enc);
        MeshFrame decoded{};
        bool ok = decode_mesh_frame(enc, key, decoded);
        if (!ok) {
            return 1;
        }
        assert(decoded.header.seq_no == frame.header.seq_no);
        good++;
    }

    const bool counts_match = radio.sent_count() == static_cast<std::size_t>(good);
    return counts_match ? 0 : 1;
}
