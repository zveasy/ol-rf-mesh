#include "mesh_encode.hpp"
#include "crypto.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace {
// Very small, hand-rolled serializer for demo. In production this would be CBOR/Proto + AES-GCM.
template <typename T>
bool write_scalar(std::array<uint8_t, kMaxMeshFrameLen>& buf, std::size_t& idx, T value) {
    const std::size_t bytes = sizeof(T);
    if (idx + bytes > buf.size()) {
        return false;
    }
    std::memcpy(buf.data() + idx, &value, bytes);
    idx += bytes;
    return true;
}

bool write_bytes(std::array<uint8_t, kMaxMeshFrameLen>& buf, std::size_t& idx, const void* data, std::size_t len) {
    if (idx + len > buf.size()) {
        return false;
    }
    std::memcpy(buf.data() + idx, data, len);
    idx += len;
    return true;
}

bool nonce_is_zero(const MeshSecurity& sec) {
    return std::all_of(sec.nonce.begin(), sec.nonce.end(), [](uint8_t b) { return b == 0; });
}

void derive_nonce(const MeshFrame& frame, std::array<uint8_t, kNonceLength>& out) {
    // Simple deterministic nonce: seq_no || first bytes of src_node_id.
    out.fill(0);
    std::memcpy(out.data(), &frame.header.seq_no, std::min(sizeof(frame.header.seq_no), out.size()));
    for (std::size_t i = 0; i < std::min(out.size() - sizeof(frame.header.seq_no), sizeof(frame.header.src_node_id)); ++i) {
        out[sizeof(frame.header.seq_no) + i] ^= static_cast<uint8_t>(frame.header.src_node_id[i]);
    }
}

struct ReplayEntry {
    char node_id[kMaxNodeIdLength];
    uint32_t last_seq;
};

constexpr std::size_t kReplaySlots = 8;
ReplayEntry g_replay_window[kReplaySlots]{};

bool check_replay_and_update(const MeshFrame& frame) {
    // Basic per-node monotonic check to avoid replays in test harness.
    for (ReplayEntry& slot : g_replay_window) {
        if (slot.node_id[0] == '\0') {
            std::snprintf(slot.node_id, sizeof(slot.node_id), "%s", frame.header.src_node_id);
            slot.last_seq = frame.header.seq_no;
            return true;
        }
        if (std::strncmp(slot.node_id, frame.header.src_node_id, sizeof(slot.node_id)) == 0) {
            if (frame.header.seq_no <= slot.last_seq) {
                return false;
            }
            slot.last_seq = frame.header.seq_no;
            return true;
        }
    }
    // No free slot; overwrite oldest slot 0 for simplicity.
    std::snprintf(g_replay_window[0].node_id, sizeof(g_replay_window[0].node_id), "%s", frame.header.src_node_id);
    g_replay_window[0].last_seq = frame.header.seq_no;
    return true;
}
} // namespace

EncodedFrame encode_mesh_frame(const MeshFrame& frame) {
    EncodedFrame out{};
    std::size_t idx = 0;

    if (!write_scalar(out.bytes, idx, frame.header.version)) return out;
    const uint8_t msg_type = static_cast<uint8_t>(frame.header.msg_type);
    if (!write_scalar(out.bytes, idx, msg_type)) return out;
    if (!write_scalar(out.bytes, idx, frame.header.ttl)) return out;
    if (!write_scalar(out.bytes, idx, frame.header.hop_count)) return out;
    if (!write_scalar(out.bytes, idx, frame.header.seq_no)) return out;

    if (!write_bytes(out.bytes, idx, frame.header.src_node_id, sizeof(frame.header.src_node_id))) return out;
    if (!write_bytes(out.bytes, idx, frame.header.dest_node_id, sizeof(frame.header.dest_node_id))) return out;

    const uint8_t encrypted = frame.security.encrypted ? 1 : 0;
    if (!write_scalar(out.bytes, idx, encrypted)) return out;
    if (!write_bytes(out.bytes, idx, frame.security.nonce.data(), frame.security.nonce.size())) return out;
    if (!write_bytes(out.bytes, idx, frame.security.auth_tag.data(), frame.security.auth_tag.size())) return out;

    // Telemetry payload (RF + GPS + Health)
    if (!write_scalar(out.bytes, idx, frame.telemetry.rf_event.timestamp_ms)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.rf_event.center_freq_hz)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.rf_event.features.avg_dbm)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.rf_event.features.peak_dbm)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.rf_event.anomaly_score)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.rf_event.model_version)) return out;

    if (!write_scalar(out.bytes, idx, frame.telemetry.gps.timestamp_ms)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.gps.latitude_deg)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.gps.longitude_deg)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.gps.altitude_m)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.gps.num_sats)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.gps.hdop)) return out;
    const uint8_t valid_fix = frame.telemetry.gps.valid_fix ? 1 : 0;
    const uint8_t jam = frame.telemetry.gps.jamming_detected ? 1 : 0;
    const uint8_t spoof = frame.telemetry.gps.spoof_detected ? 1 : 0;
    if (!write_scalar(out.bytes, idx, valid_fix)) return out;
    if (!write_scalar(out.bytes, idx, jam)) return out;
    if (!write_scalar(out.bytes, idx, spoof)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.gps.cn0_db_hz_avg)) return out;

    if (!write_scalar(out.bytes, idx, frame.telemetry.health.timestamp_ms)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.health.battery_v)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.health.temp_c)) return out;
    if (!write_scalar(out.bytes, idx, frame.telemetry.health.imu_tilt_deg)) return out;
    const uint8_t tamper = frame.telemetry.health.tamper_flag ? 1 : 0;
    if (!write_scalar(out.bytes, idx, tamper)) return out;

    // Fault + OTA status
    const uint8_t fault_active = frame.fault.fault_active ? 1 : 0;
    if (!write_scalar(out.bytes, idx, fault_active)) return out;
    if (!write_scalar(out.bytes, idx, frame.fault.counters.watchdog_resets)) return out;
    if (!write_scalar(out.bytes, idx, frame.fault.counters.ota_failures)) return out;
    if (!write_scalar(out.bytes, idx, frame.fault.counters.tamper_events)) return out;

    if (!write_scalar(out.bytes, idx, static_cast<uint8_t>(frame.ota.state))) return out;
    if (!write_scalar(out.bytes, idx, frame.ota.current_offset)) return out;
    if (!write_scalar(out.bytes, idx, frame.ota.total_size)) return out;
    const uint8_t ota_sig = frame.ota.signature_valid ? 1 : 0;
    if (!write_scalar(out.bytes, idx, ota_sig)) return out;

    // Routing payload (optional)
    if (!write_scalar(out.bytes, idx, frame.routing.epoch_ms)) return out;
    if (!write_scalar(out.bytes, idx, static_cast<uint8_t>(frame.routing.entry_count))) return out;
    for (std::size_t i = 0; i < frame.routing.entry_count && idx < out.bytes.size(); ++i) {
        if (!write_bytes(out.bytes, idx, frame.routing.entries[i].neighbor_id, sizeof(frame.routing.entries[i].neighbor_id))) break;
        if (!write_scalar(out.bytes, idx, frame.routing.entries[i].rssi_dbm)) break;
        if (!write_scalar(out.bytes, idx, frame.routing.entries[i].link_quality)) break;
        if (!write_scalar(out.bytes, idx, frame.routing.entries[i].cost)) break;
    }

    out.len = idx;
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
    if (entry_count > kMaxRoutes) return false;
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
    if (enc.len < kNonceLength + kAuthTagLength) return false;
    const uint8_t* nonce = enc.bytes.data();
    const uint8_t* tag = enc.bytes.data() + kNonceLength;
    const uint8_t* ciphertext = enc.bytes.data() + kNonceLength + kAuthTagLength;
    clear.len = enc.len - (kNonceLength + kAuthTagLength);
    if (clear.len > clear.bytes.size()) return false;
    // Layout: nonce || auth_tag || ciphertext
    AesGcmResult res = aes_gcm_decrypt(
        ciphertext,
        clear.len,
        key,
        nonce,
        kNonceLength,
        tag,
        kAuthTagLength,
        clear.bytes.data(),
        clear.bytes.size()
    );
    if (!res.ok) return false;
    clear.len = res.ciphertext_len;
    if (!decode_mesh_frame_clear(clear, out)) {
        return false;
    }
    return check_replay_and_update(out);
}

EncryptedFrame encrypt_mesh_frame(const MeshFrame& frame, const AesGcmKey& key) {
    MeshFrame framed = frame;
    if (nonce_is_zero(framed.security)) {
        derive_nonce(framed, framed.security.nonce);
    }

    EncodedFrame clear = encode_mesh_frame(framed);
    EncryptedFrame out{};
    // Layout: [nonce || auth_tag || ciphertext]
    const std::size_t overhead = kNonceLength + kAuthTagLength;
    if (clear.len == 0 || clear.len + overhead > out.bytes.size()) {
        out.len = 0;
        return out;
    }

    std::memcpy(out.bytes.data(), framed.security.nonce.data(), framed.security.nonce.size());

    AesGcmResult res = aes_gcm_encrypt(
        clear.bytes.data(),
        clear.len,
        key,
        framed.security.nonce.data(),
        framed.security.nonce.size(),
        out.bytes.data() + overhead, // reserve space for nonce+tag
        out.bytes.size() - overhead,
        out.bytes.data() + kNonceLength,
        kAuthTagLength
    );
    out.len = res.ok ? res.ciphertext_len + overhead : 0;
    return out;
}
