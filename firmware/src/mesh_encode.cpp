#include "mesh_encode.hpp"
#include "crypto.hpp"
#include <cstring>

namespace {
// Very small, hand-rolled serializer for demo. In production this would be CBOR/Proto + AES-GCM.
template <typename T>
void write_scalar(std::array<uint8_t, kMaxMeshFrameLen>& buf, std::size_t& idx, T value) {
    const std::size_t bytes = sizeof(T);
    if (idx + bytes > buf.size()) {
        return;
    }
    std::memcpy(buf.data() + idx, &value, bytes);
    idx += bytes;
}

void write_bytes(std::array<uint8_t, kMaxMeshFrameLen>& buf, std::size_t& idx, const void* data, std::size_t len) {
    if (idx + len > buf.size()) {
        return;
    }
    std::memcpy(buf.data() + idx, data, len);
    idx += len;
}
} // namespace

EncodedFrame encode_mesh_frame(const MeshFrame& frame) {
    EncodedFrame out{};
    std::size_t idx = 0;

    write_scalar(out.bytes, idx, frame.header.version);
    const uint8_t msg_type = static_cast<uint8_t>(frame.header.msg_type);
    write_scalar(out.bytes, idx, msg_type);
    write_scalar(out.bytes, idx, frame.header.ttl);
    write_scalar(out.bytes, idx, frame.header.hop_count);
    write_scalar(out.bytes, idx, frame.header.seq_no);

    write_bytes(out.bytes, idx, frame.header.src_node_id, sizeof(frame.header.src_node_id));
    write_bytes(out.bytes, idx, frame.header.dest_node_id, sizeof(frame.header.dest_node_id));

    const uint8_t encrypted = frame.security.encrypted ? 1 : 0;
    write_scalar(out.bytes, idx, encrypted);
    write_bytes(out.bytes, idx, frame.security.nonce.data(), frame.security.nonce.size());
    write_bytes(out.bytes, idx, frame.security.auth_tag.data(), frame.security.auth_tag.size());

    // Telemetry payload (RF + GPS + Health)
    write_scalar(out.bytes, idx, frame.telemetry.rf_event.timestamp_ms);
    write_scalar(out.bytes, idx, frame.telemetry.rf_event.center_freq_hz);
    write_scalar(out.bytes, idx, frame.telemetry.rf_event.features.avg_dbm);
    write_scalar(out.bytes, idx, frame.telemetry.rf_event.features.peak_dbm);
    write_scalar(out.bytes, idx, frame.telemetry.rf_event.anomaly_score);
    write_scalar(out.bytes, idx, frame.telemetry.rf_event.model_version);

    write_scalar(out.bytes, idx, frame.telemetry.gps.timestamp_ms);
    write_scalar(out.bytes, idx, frame.telemetry.gps.latitude_deg);
    write_scalar(out.bytes, idx, frame.telemetry.gps.longitude_deg);
    write_scalar(out.bytes, idx, frame.telemetry.gps.altitude_m);
    write_scalar(out.bytes, idx, frame.telemetry.gps.num_sats);
    write_scalar(out.bytes, idx, frame.telemetry.gps.hdop);
    const uint8_t valid_fix = frame.telemetry.gps.valid_fix ? 1 : 0;
    const uint8_t jam = frame.telemetry.gps.jamming_detected ? 1 : 0;
    const uint8_t spoof = frame.telemetry.gps.spoof_detected ? 1 : 0;
    write_scalar(out.bytes, idx, valid_fix);
    write_scalar(out.bytes, idx, jam);
    write_scalar(out.bytes, idx, spoof);
    write_scalar(out.bytes, idx, frame.telemetry.gps.cn0_db_hz_avg);

    write_scalar(out.bytes, idx, frame.telemetry.health.timestamp_ms);
    write_scalar(out.bytes, idx, frame.telemetry.health.battery_v);
    write_scalar(out.bytes, idx, frame.telemetry.health.temp_c);
    write_scalar(out.bytes, idx, frame.telemetry.health.imu_tilt_deg);
    const uint8_t tamper = frame.telemetry.health.tamper_flag ? 1 : 0;
    write_scalar(out.bytes, idx, tamper);

    // Fault + OTA status
    const uint8_t fault_active = frame.fault.fault_active ? 1 : 0;
    write_scalar(out.bytes, idx, fault_active);
    write_scalar(out.bytes, idx, frame.fault.counters.watchdog_resets);
    write_scalar(out.bytes, idx, frame.fault.counters.ota_failures);
    write_scalar(out.bytes, idx, frame.fault.counters.tamper_events);

    write_scalar(out.bytes, idx, static_cast<uint8_t>(frame.ota.state));
    write_scalar(out.bytes, idx, frame.ota.current_offset);
    write_scalar(out.bytes, idx, frame.ota.total_size);
    const uint8_t ota_sig = frame.ota.signature_valid ? 1 : 0;
    write_scalar(out.bytes, idx, ota_sig);

    // Routing payload (optional)
    write_scalar(out.bytes, idx, frame.routing.epoch_ms);
    write_scalar(out.bytes, idx, static_cast<uint8_t>(frame.routing.entry_count));
    for (std::size_t i = 0; i < frame.routing.entry_count && idx < out.bytes.size(); ++i) {
        write_bytes(out.bytes, idx, frame.routing.entries[i].neighbor_id, sizeof(frame.routing.entries[i].neighbor_id));
        write_scalar(out.bytes, idx, frame.routing.entries[i].rssi_dbm);
        write_scalar(out.bytes, idx, frame.routing.entries[i].link_quality);
        write_scalar(out.bytes, idx, frame.routing.entries[i].cost);
    }

    out.len = idx;
    return out;
}

EncryptedFrame encrypt_mesh_frame(const MeshFrame& frame, const AesGcmKey& key) {
    EncryptedFrame out{};
    const EncodedFrame clear = encode_mesh_frame(frame);

    uint8_t ciphertext[kMaxCipherLen]{0};
    AesGcmResult res = aes_gcm_encrypt(
        clear.bytes.data(),
        clear.len,
        key,
        frame.security.nonce.data(),
        frame.security.nonce.size(),
        ciphertext,
        sizeof(ciphertext),
        out.bytes.data(),
        kAuthTagLength
    );

    // Layout: [auth_tag||ciphertext]
    const std::size_t offset = kAuthTagLength;
    if (res.ok) {
        std::memcpy(out.bytes.data() + offset, ciphertext, res.ciphertext_len);
        out.len = offset + res.ciphertext_len;
    } else {
        out.len = 0;
    }

    return out;
}

static bool decode_mesh_frame_clear(const EncodedFrame& enc, MeshFrame& frame) {
    std::size_t idx = 0;
    auto read_scalar = [&](auto& dst) -> bool {
        using T = std::decay_t<decltype(dst)>;
        if (idx + sizeof(T) > enc.len) return false;
        std::memcpy(&dst, enc.bytes.data() + idx, sizeof(T));
        idx += sizeof(T);
        return true;
    };

    read_scalar(frame.header.version);
    uint8_t msg_type = 0;
    read_scalar(msg_type);
    frame.header.msg_type = static_cast<MeshMsgType>(msg_type);
    read_scalar(frame.header.ttl);
    read_scalar(frame.header.hop_count);
    read_scalar(frame.header.seq_no);
    if (idx + sizeof(frame.header.src_node_id) + sizeof(frame.header.dest_node_id) > enc.len) return false;
    std::memcpy(frame.header.src_node_id, enc.bytes.data() + idx, sizeof(frame.header.src_node_id));
    idx += sizeof(frame.header.src_node_id);
    std::memcpy(frame.header.dest_node_id, enc.bytes.data() + idx, sizeof(frame.header.dest_node_id));
    idx += sizeof(frame.header.dest_node_id);

    uint8_t encrypted = 0;
    read_scalar(encrypted);
    if (idx + kNonceLength + kAuthTagLength > enc.len) return false;
    std::memcpy(frame.security.nonce.data(), enc.bytes.data() + idx, kNonceLength);
    idx += kNonceLength;
    std::memcpy(frame.security.auth_tag.data(), enc.bytes.data() + idx, kAuthTagLength);
    idx += kAuthTagLength;
    frame.security.encrypted = encrypted != 0;

    read_scalar(frame.telemetry.rf_event.timestamp_ms);
    read_scalar(frame.telemetry.rf_event.center_freq_hz);
    read_scalar(frame.telemetry.rf_event.features.avg_dbm);
    read_scalar(frame.telemetry.rf_event.features.peak_dbm);
    read_scalar(frame.telemetry.rf_event.anomaly_score);
    read_scalar(frame.telemetry.rf_event.model_version);

    read_scalar(frame.telemetry.gps.timestamp_ms);
    read_scalar(frame.telemetry.gps.latitude_deg);
    read_scalar(frame.telemetry.gps.longitude_deg);
    read_scalar(frame.telemetry.gps.altitude_m);
    read_scalar(frame.telemetry.gps.num_sats);
    read_scalar(frame.telemetry.gps.hdop);
    uint8_t valid_fix = 0, jam = 0, spoof = 0;
    read_scalar(valid_fix);
    read_scalar(jam);
    read_scalar(spoof);
    frame.telemetry.gps.valid_fix = valid_fix != 0;
    frame.telemetry.gps.jamming_detected = jam != 0;
    frame.telemetry.gps.spoof_detected = spoof != 0;
    read_scalar(frame.telemetry.gps.cn0_db_hz_avg);

    read_scalar(frame.telemetry.health.timestamp_ms);
    read_scalar(frame.telemetry.health.battery_v);
    read_scalar(frame.telemetry.health.temp_c);
    read_scalar(frame.telemetry.health.imu_tilt_deg);
    uint8_t tamper = 0;
    read_scalar(tamper);
    frame.telemetry.health.tamper_flag = tamper != 0;

    uint8_t fault_active = 0;
    read_scalar(fault_active);
    frame.fault.fault_active = fault_active != 0;
    read_scalar(frame.fault.counters.watchdog_resets);
    read_scalar(frame.fault.counters.ota_failures);
    read_scalar(frame.fault.counters.tamper_events);

    uint8_t ota_state = 0;
    read_scalar(ota_state);
    frame.ota.state = static_cast<OtaState>(ota_state);
    read_scalar(frame.ota.current_offset);
    read_scalar(frame.ota.total_size);
    uint8_t ota_sig = 0;
    read_scalar(ota_sig);
    frame.ota.signature_valid = ota_sig != 0;

    read_scalar(frame.routing.epoch_ms);
    uint8_t entry_count = 0;
    read_scalar(entry_count);
    frame.routing.entry_count = entry_count;
    for (std::size_t i = 0; i < frame.routing.entry_count && idx < enc.len; ++i) {
        if (idx + sizeof(frame.routing.entries[i].neighbor_id) > enc.len) return false;
        std::memcpy(frame.routing.entries[i].neighbor_id, enc.bytes.data() + idx, sizeof(frame.routing.entries[i].neighbor_id));
        idx += sizeof(frame.routing.entries[i].neighbor_id);
        read_scalar(frame.routing.entries[i].rssi_dbm);
        read_scalar(frame.routing.entries[i].link_quality);
        read_scalar(frame.routing.entries[i].cost);
    }

    return true;
}

bool decode_mesh_frame(const EncryptedFrame& enc, const AesGcmKey& key, MeshFrame& out) {
    EncodedFrame clear{};
    clear.len = enc.len > kAuthTagLength ? enc.len - kAuthTagLength : 0;
    if (clear.len > clear.bytes.size()) return false;
    // Layout: auth_tag || ciphertext
    AesGcmResult res = aes_gcm_decrypt(
        enc.bytes.data() + kAuthTagLength,
        clear.len,
        key,
        nullptr,
        0,
        enc.bytes.data(),
        kAuthTagLength,
        clear.bytes.data(),
        clear.bytes.size()
    );
    if (!res.ok) return false;
    clear.len = res.ciphertext_len;
    return decode_mesh_frame_clear(clear, out);
}
