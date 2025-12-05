#include "mesh_encode.hpp"
#include "telemetry.hpp"
#include "mesh.hpp"

#include <cassert>
#include <string>

static MeshFrame make_golden_frame() {
    MeshFrame f{};
    f.header.version = 1;
    f.header.msg_type = MeshMsgType::Telemetry;
    f.header.ttl = 3;
    f.header.hop_count = 0;
    f.header.seq_no = 7;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "node-gold");
    std::snprintf(f.header.dest_node_id, sizeof(f.header.dest_node_id), "gw");

    f.security.encrypted = true;
    f.security.nonce = {0,1,2,3,4,5,6,7,8,9,10,11};
    f.security.auth_tag.fill(0xAA);

    f.counters.tx_counter = 7;
    f.counters.replay_window = 1;

    f.telemetry.rf_event.timestamp_ms = 1234;
    f.telemetry.rf_event.center_freq_hz = 915000000;
    f.telemetry.rf_event.features.avg_dbm = -55.5f;
    f.telemetry.rf_event.features.peak_dbm = -42.0f;
    f.telemetry.rf_event.anomaly_score = 0.12f;
    f.telemetry.rf_event.model_version = 2;

    f.telemetry.gps.timestamp_ms = 1234;
    f.telemetry.gps.latitude_deg = 1.23f;
    f.telemetry.gps.longitude_deg = 4.56f;
    f.telemetry.gps.altitude_m = 7.89f;
    f.telemetry.gps.num_sats = 8;
    f.telemetry.gps.hdop = 1.1f;
    f.telemetry.gps.valid_fix = true;
    f.telemetry.gps.jamming_detected = false;
    f.telemetry.gps.spoof_detected = false;
    f.telemetry.gps.cn0_db_hz_avg = 38.0f;

    f.telemetry.health.timestamp_ms = 1234;
    f.telemetry.health.battery_v = 3.8f;
    f.telemetry.health.temp_c = 26.0f;
    f.telemetry.health.imu_tilt_deg = 0.4f;
    f.telemetry.health.tamper_flag = false;

    f.routing.epoch_ms = 1234;
    f.routing.version = 9;
    f.routing.entry_count = 1;
    std::snprintf(f.routing.entries[0].neighbor_id, sizeof(f.routing.entries[0].neighbor_id), "p1");
    f.routing.entries[0].rssi_dbm = -60;
    f.routing.entries[0].link_quality = 180;
    f.routing.entries[0].cost = 1;

    f.fault.fault_active = false;
    f.ota.state = OtaState::Idle;
    return f;
}

static std::string to_hex(const EncryptedFrame& enc) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(enc.len * 2);
    for (std::size_t i = 0; i < enc.len; ++i) {
        uint8_t b = enc.bytes[i];
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

int main() {
    AesGcmKey key{};
    key.bytes.fill(0x11);

    MeshFrame f = make_golden_frame();
    EncryptedFrame enc = encrypt_mesh_frame(f, key);
    assert(enc.len > 0);

    const std::string hex = to_hex(enc);
    static const std::string golden =
        "000102030405060708090A0B6DA4D65112F7DF92000000000000000055E2D0A48D5413303DB48015BEEE11C036A41D3957D4052B1B9A5CD6B4468F004A3095E2640BBB8026216CB9731F14FACBD4526453F6BECB04BC81E443BCBED25E1C7E8BF1C1FB13BB9AB8D2A1C9E5CC7DC71014F8024A043D00567D18AB88349DB9EF5F40888955289F5A1AB00AB6968E635D091FDEBCEE55EBBAA4F4823C5BB015B070C5948B916391374C9FDFDC414DC18F508E00862759D320E9CFA69C9F672D5A9B8C16D4052AC7EBF1131134EFE460BAC04381BE60A378722F17D33E7E5DBBAFC1159DF80B9F3C018432F0A1DCB824D952F3976558B6F6828652470614EED07B3E";
    assert(hex == golden);

    MeshFrame decoded{};
    bool ok = decode_mesh_frame(enc, key, decoded);
    if (!ok) {
        return 1;
    }
    assert(decoded.counters.tx_counter == f.counters.tx_counter);
    assert(decoded.counters.replay_window == f.counters.replay_window);
    assert(std::string(decoded.routing.entries[0].neighbor_id) == "p1");
    return 0;
}
