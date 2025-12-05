#include "mesh_encode.hpp"
#include "mock_radio.hpp"
#include "telemetry.hpp"
#include "mesh.hpp"

#include <cassert>
#include <cstring>

static MeshFrame make_sample(uint32_t seq) {
    MeshFrame f{};
    f.header.version = 1;
    f.header.msg_type = MeshMsgType::Telemetry;
    f.header.ttl = 3;
    f.header.hop_count = 0;
    f.header.seq_no = seq;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "node-%u", seq);
    std::snprintf(f.header.dest_node_id, sizeof(f.header.dest_node_id), "gw");
    f.telemetry.rf_event.timestamp_ms = seq;
    f.telemetry.rf_event.center_freq_hz = 915000000;
    f.telemetry.rf_event.features.avg_dbm = -55.5f;
    f.telemetry.rf_event.features.peak_dbm = -42.0f;
    f.telemetry.rf_event.anomaly_score = 0.2f;
    f.telemetry.rf_event.model_version = 1;
    f.telemetry.gps.valid_fix = true;
    f.telemetry.health.battery_v = 3.8f;
    f.routing.entry_count = 1;
    f.routing.version = 42;
    f.routing.epoch_ms = 1234;
    f.routing.entries[0].cost = 2;
    std::snprintf(f.routing.entries[0].neighbor_id, sizeof(f.routing.entries[0].neighbor_id), "p1");
    f.counters.tx_counter = seq;
    f.counters.replay_window = 0;
    return f;
}

int main() {
    AesGcmKey key{};
    key.bytes.fill(0x44);
    MockRadio radio;

    unsigned delivered = 0;
    radio.set_receive_handler([&](const EncryptedFrame& enc) {
        MeshFrame decoded{};
        bool ok = decode_mesh_frame(enc, key, decoded);
        (void)ok;
        assert(ok);
        assert(decoded.header.seq_no == delivered + 1);
        assert(std::string(decoded.header.dest_node_id) == std::string("gw"));
        assert(decoded.routing.version == 42);
        assert(decoded.routing.entry_count == 1);
        assert(std::string(decoded.routing.entries[0].neighbor_id) == "p1");
        delivered++;
    });

    for (uint32_t seq = 1; seq <= 3; ++seq) {
        MeshFrame f = make_sample(seq);
        EncryptedFrame enc = encrypt_mesh_frame(f, key);
        radio.enqueue_to_air(enc);
    }
    radio.pump_air(0.0f, 77);

    assert(delivered == 3);
    return 0;
}
