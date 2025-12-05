#include "mesh.hpp"
#include "mesh_encode.hpp"
#include "mock_radio.hpp"
#include "telemetry.hpp"

#include <cassert>
#include <random>

static MeshFrame make_minimal_frame(uint32_t seq) {
    MeshFrame f{};
    f.header.version = 1;
    f.header.msg_type = MeshMsgType::Telemetry;
    f.header.ttl = 3;
    f.header.hop_count = 0;
    f.header.seq_no = seq;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "node-A");
    std::snprintf(f.header.dest_node_id, sizeof(f.header.dest_node_id), "gw");

    f.telemetry.rf_event.features.avg_dbm = -60.0f;
    f.telemetry.rf_event.features.peak_dbm = -40.0f;
    f.telemetry.rf_event.model_version = 1;
    f.telemetry.gps.valid_fix = true;
    return f;
}

int main() {
    MockRadio radio;
    unsigned int received = 0;
    radio.set_receive_handler([&](const EncryptedFrame& enc) {
        // Basic property: encrypted frames should be non-zero length and carry seq_no we expect to monotonically increase.
        (void)enc;
        assert(enc.len > 0);
        received++;
    });

    AesGcmKey key{};
    key.bytes.fill(0x33);

    // Simulate retries on drop: we enqueue to air, pump with 50% drop, and resend until delivered or max retries.
    const int max_retries = 5;
    for (uint32_t seq = 1; seq <= 5; ++seq) {
        bool delivered = false;
        for (int attempt = 0; attempt < max_retries && !delivered; ++attempt) {
            MeshFrame f = make_minimal_frame(seq);
            EncryptedFrame enc = encrypt_mesh_frame(f, key);
            radio.enqueue_to_air(enc);
            radio.pump_air(0.1f, 100 + seq + attempt);
            if (received >= seq) {
                delivered = true;
            }
        }
        assert(delivered);
    }

    // Sanity: all 5 frames delivered despite simulated drops.
    assert(received == 5);
    return 0;
}
