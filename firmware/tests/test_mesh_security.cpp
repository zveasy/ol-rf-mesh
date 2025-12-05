#include "mesh_encode.hpp"
#include "telemetry.hpp"
#include "mesh.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>

static MeshFrame make_frame(uint32_t seq, const char* node_id) {
    MeshFrame f{};
    f.header.version = 1;
    f.header.msg_type = MeshMsgType::Telemetry;
    f.header.ttl = 3;
    f.header.hop_count = 0;
    f.header.seq_no = seq;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "%s", node_id);
    std::snprintf(f.header.dest_node_id, sizeof(f.header.dest_node_id), "gw");
    f.telemetry.rf_event.features.avg_dbm = -55.0f;
    f.telemetry.rf_event.features.peak_dbm = -42.0f;
    f.telemetry.gps.valid_fix = true;
    f.security.encrypted = true;
    return f;
}

int main() {
    AesGcmKey key{};
    key.bytes.fill(0xAA);

    MeshFrame frame = make_frame(1, "node-sec");
    EncryptedFrame enc = encrypt_mesh_frame(frame, key);
    assert(enc.len > 0);

    MeshFrame decoded{};
    bool ok = decode_mesh_frame(enc, key, decoded);
    (void)ok;
    assert(ok);
    assert(decoded.header.seq_no == frame.header.seq_no);

    // Tamper with ciphertext and expect failure.
    EncryptedFrame tampered = enc;
    tampered.bytes[tampered.len - 1] ^= 0xFF;
    MeshFrame out_bad{};
    bool bad = decode_mesh_frame(tampered, key, out_bad);
    (void)bad;
    assert(!bad);

    // Replay detection: same frame should be rejected on second decode.
    MeshFrame replay_out{};
    bool replay_ok = decode_mesh_frame(enc, key, replay_out);
    (void)replay_ok;
    assert(!replay_ok);

    return 0;
}
