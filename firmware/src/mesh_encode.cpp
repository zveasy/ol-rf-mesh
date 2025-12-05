#include "mesh_encode.hpp"
#include "crypto.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace {
// Minimal CBOR writer/reader tailored to the mesh schema.
constexpr uint8_t kMajorUInt = 0u;
constexpr uint8_t kMajorBytes = 2u;
constexpr uint8_t kMajorText = 3u;
constexpr uint8_t kMajorArray = 4u;
constexpr uint8_t kMajorMap = 5u;
constexpr uint8_t kMajorSimple = 7u;

bool nonce_is_zero(const MeshSecurity& sec) {
    return std::all_of(sec.nonce.begin(), sec.nonce.end(), [](uint8_t b) { return b == 0; });
}

void derive_nonce(const MeshFrame& frame, std::array<uint8_t, kNonceLength>& out) {
    // Deterministic nonce: seq_no || first bytes of src_node_id.
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

struct CborWriter {
    std::array<uint8_t, kMaxMeshFrameLen>& buf;
    std::size_t idx = 0;

    bool write_type(uint8_t major, uint64_t val) {
        uint8_t tmp[9]{};
        std::size_t len = 1;
        if (val < 24) {
            tmp[0] = static_cast<uint8_t>((major << 5) | val);
            len = 1;
        } else if (val <= 0xFF) {
            tmp[0] = static_cast<uint8_t>((major << 5) | 24);
            tmp[1] = static_cast<uint8_t>(val);
            len = 2;
        } else if (val <= 0xFFFF) {
            tmp[0] = static_cast<uint8_t>((major << 5) | 25);
            tmp[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
            tmp[2] = static_cast<uint8_t>(val & 0xFF);
            len = 3;
        } else if (val <= 0xFFFFFFFF) {
            tmp[0] = static_cast<uint8_t>((major << 5) | 26);
            tmp[1] = static_cast<uint8_t>((val >> 24) & 0xFF);
            tmp[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
            tmp[3] = static_cast<uint8_t>((val >> 8) & 0xFF);
            tmp[4] = static_cast<uint8_t>(val & 0xFF);
            len = 5;
        } else {
            return false;
        }
        if (idx + len > buf.size()) return false;
        std::memcpy(buf.data() + idx, tmp, len);
        idx += len;
        return true;
    }
    bool write_uint(uint32_t v) { return write_type(kMajorUInt, v); }
    bool write_bytes(const uint8_t* data, std::size_t len) {
        if (!write_type(kMajorBytes, len)) return false;
        if (idx + len > buf.size()) return false;
        std::memcpy(buf.data() + idx, data, len);
        idx += len;
        return true;
    }
    bool write_text(const char* data, std::size_t len) {
        if (!write_type(kMajorText, len)) return false;
        if (idx + len > buf.size()) return false;
        std::memcpy(buf.data() + idx, data, len);
        idx += len;
        return true;
    }
    bool write_float(float v) {
        if (!write_type(kMajorSimple, 26)) return false; // 32-bit float
        if (idx + sizeof(float) > buf.size()) return false;
        std::memcpy(buf.data() + idx, &v, sizeof(float));
        idx += sizeof(float);
        return true;
    }
    bool write_map_start(std::size_t count) { return write_type(kMajorMap, count); }
    bool write_array_start(std::size_t count) { return write_type(kMajorArray, count); }
};

struct CborReader {
    const uint8_t* data;
    std::size_t len;
    std::size_t idx = 0;

    bool read_type(uint8_t& major, uint64_t& val) {
        if (idx >= len) return false;
        const uint8_t ib = data[idx++];
        major = ib >> 5;
        const uint8_t ai = ib & 0x1F;
        if (ai < 24) {
            val = ai;
            return true;
        }
        if (ai == 24) {
            if (idx >= len) return false;
            val = data[idx++];
            return true;
        }
        if (ai == 25) {
            if (idx + 1 >= len) return false;
            val = static_cast<uint64_t>(data[idx] << 8 | data[idx + 1]);
            idx += 2;
            return true;
        }
        if (ai == 26) {
            if (idx + 3 >= len) return false;
            val = (static_cast<uint64_t>(data[idx]) << 24) |
                  (static_cast<uint64_t>(data[idx + 1]) << 16) |
                  (static_cast<uint64_t>(data[idx + 2]) << 8) |
                  static_cast<uint64_t>(data[idx + 3]);
            idx += 4;
            return true;
        }
        return false;
    }
    bool read_uint(uint32_t& out) {
        uint8_t major = 0; uint64_t val = 0;
        if (!read_type(major, val)) return false;
        if (major != kMajorUInt) return false;
        out = static_cast<uint32_t>(val);
        return true;
    }
    bool read_bytes(uint8_t* out_buf, std::size_t max_len, std::size_t& out_len) {
        uint8_t major = 0; uint64_t val = 0;
        if (!read_type(major, val)) return false;
        if (major != kMajorBytes) return false;
        if (idx + val > len || val > max_len) return false;
        std::memcpy(out_buf, data + idx, static_cast<std::size_t>(val));
        out_len = static_cast<std::size_t>(val);
        idx += val;
        return true;
    }
    bool read_text(char* out_buf, std::size_t max_len) {
        uint8_t major = 0; uint64_t val = 0;
        if (!read_type(major, val)) return false;
        if (major != kMajorText) return false;
        if (idx + val > len || val >= max_len) return false;
        std::memcpy(out_buf, data + idx, static_cast<std::size_t>(val));
        out_buf[val] = '\0';
        idx += val;
        return true;
    }
    bool read_float(float& out) {
        uint8_t major = 0; uint64_t val = 0;
        if (!read_type(major, val)) return false;
        if (major != kMajorSimple || val != 26) return false;
        if (idx + sizeof(float) > len) return false;
        std::memcpy(&out, data + idx, sizeof(float));
        idx += sizeof(float);
        return true;
    }
    bool read_array_len(std::size_t& out_len) {
        uint8_t major = 0; uint64_t val = 0;
        if (!read_type(major, val)) return false;
        if (major != kMajorArray) return false;
        out_len = static_cast<std::size_t>(val);
        return true;
    }
    bool read_map_len(std::size_t& out_len) {
        uint8_t major = 0; uint64_t val = 0;
        if (!read_type(major, val)) return false;
        if (major != kMajorMap) return false;
        out_len = static_cast<std::size_t>(val);
        return true;
    }
    bool skip_value();
};

bool CborReader::skip_value() {
    uint8_t major = 0; uint64_t val = 0;
    const std::size_t start = idx;
    if (!read_type(major, val)) return false;
    switch (major) {
        case kMajorUInt:
        case 1: // negative ints unused
            return true;
        case kMajorBytes:
        case kMajorText:
            if (idx + val > len) return false;
            idx += val;
            return true;
        case kMajorArray:
            for (std::size_t i = 0; i < val; ++i) {
                if (!skip_value()) return false;
            }
            return true;
        case kMajorMap:
            for (std::size_t i = 0; i < val; ++i) {
                if (!skip_value()) return false; // key
                if (!skip_value()) return false; // value
            }
            return true;
        case kMajorSimple:
            if (val == 26) {
                if (idx + sizeof(float) > len) return false;
                idx += sizeof(float);
                return true;
            }
            return false;
        default:
            idx = start;
            return false;
    }
}

bool encode_routing(CborWriter& w, const MeshRoutingPayload& r) {
    if (!w.write_map_start(4)) return false;
    if (!w.write_uint(1) || !w.write_uint(r.epoch_ms)) return false;
    if (!w.write_uint(2) || !w.write_uint(r.version)) return false;
    if (!w.write_uint(3)) return false;
    if (!w.write_array_start(r.entry_count)) return false;
    for (std::size_t i = 0; i < r.entry_count; ++i) {
        const auto& e = r.entries[i];
        if (!w.write_map_start(4)) return false;
        if (!w.write_uint(1) || !w.write_text(e.neighbor_id, strnlen(e.neighbor_id, kMaxNodeIdLength))) return false;
        if (!w.write_uint(2) || !w.write_uint(static_cast<uint32_t>(static_cast<int32_t>(e.rssi_dbm) & 0xFF))) return false;
        if (!w.write_uint(3) || !w.write_uint(e.link_quality)) return false;
        if (!w.write_uint(4) || !w.write_uint(e.cost)) return false;
    }
    if (!w.write_uint(4) || !w.write_uint(static_cast<uint32_t>(r.entry_count))) return false;
    return true;
}

bool decode_routing(CborReader& r, MeshRoutingPayload& out) {
    std::size_t map_len = 0;
    if (!r.read_map_len(map_len)) return false;
    for (std::size_t i = 0; i < map_len; ++i) {
        uint32_t key = 0;
        if (!r.read_uint(key)) return false;
        switch (key) {
            case 1: r.read_uint(out.epoch_ms); break;
            case 2: r.read_uint(out.version); break;
            case 3: {
                std::size_t arr_len = 0;
                if (!r.read_array_len(arr_len)) return false;
                out.entry_count = std::min<std::size_t>(arr_len, kMaxRoutes);
                for (std::size_t j = 0; j < out.entry_count; ++j) {
                    std::size_t ent_map = 0;
                    if (!r.read_map_len(ent_map)) return false;
                    for (std::size_t k = 0; k < ent_map; ++k) {
                        uint32_t ent_key = 0;
                        if (!r.read_uint(ent_key)) return false;
                        switch (ent_key) {
                            case 1: r.read_text(out.entries[j].neighbor_id, kMaxNodeIdLength); break;
                            case 2: {
                                uint32_t tmp = 0; r.read_uint(tmp);
                                out.entries[j].rssi_dbm = static_cast<int8_t>(tmp & 0xFF);
                                break;
                            }
                            case 3: { uint32_t tmp = 0; r.read_uint(tmp); out.entries[j].link_quality = static_cast<uint8_t>(tmp & 0xFF); break; }
                            case 4: { uint32_t tmp = 0; r.read_uint(tmp); out.entries[j].cost = static_cast<uint8_t>(tmp & 0xFF); break; }
                            default: r.skip_value(); break;
                        }
                    }
                }
                // Skip any remaining entries beyond capacity.
                for (std::size_t j = out.entry_count; j < arr_len; ++j) {
                    r.skip_value();
                }
                break;
            }
            case 4: {
                uint32_t cnt = 0; r.read_uint(cnt);
                out.entry_count = std::min<std::size_t>(cnt, out.entry_count);
                break;
            }
            default:
                r.skip_value();
                break;
        }
    }
    return true;
}
} // namespace

EncodedFrame encode_mesh_frame(const MeshFrame& frame) {
    EncodedFrame out{};
    CborWriter w{out.bytes};

    // Top-level map keys: 1=header,2=security,3=counters,4=rf,5=gps,6=health,7=routing,8=fault,9=ota
    if (!w.write_map_start(9)) return out;

    // Header
    if (!w.write_uint(1) || !w.write_map_start(7)) return out;
    if (!w.write_uint(1) || !w.write_uint(frame.header.version)) return out;
    if (!w.write_uint(2) || !w.write_uint(static_cast<uint8_t>(frame.header.msg_type))) return out;
    if (!w.write_uint(3) || !w.write_uint(frame.header.ttl)) return out;
    if (!w.write_uint(4) || !w.write_uint(frame.header.hop_count)) return out;
    if (!w.write_uint(5) || !w.write_uint(frame.header.seq_no)) return out;
    if (!w.write_uint(6) || !w.write_text(frame.header.src_node_id, strnlen(frame.header.src_node_id, kMaxNodeIdLength))) return out;
    if (!w.write_uint(7) || !w.write_text(frame.header.dest_node_id, strnlen(frame.header.dest_node_id, kMaxNodeIdLength))) return out;

    // Security
    if (!w.write_uint(2) || !w.write_map_start(3)) return out;
    if (!w.write_uint(1) || !w.write_uint(frame.security.encrypted ? 1 : 0)) return out;
    if (!w.write_uint(2) || !w.write_bytes(frame.security.nonce.data(), frame.security.nonce.size())) return out;
    if (!w.write_uint(3) || !w.write_bytes(frame.security.auth_tag.data(), frame.security.auth_tag.size())) return out;

    // Counters
    if (!w.write_uint(3) || !w.write_map_start(2)) return out;
    if (!w.write_uint(1) || !w.write_uint(frame.counters.tx_counter)) return out;
    if (!w.write_uint(2) || !w.write_uint(frame.counters.replay_window)) return out;

    // RF
    if (!w.write_uint(4) || !w.write_map_start(6)) return out;
    if (!w.write_uint(1) || !w.write_uint(frame.telemetry.rf_event.timestamp_ms)) return out;
    if (!w.write_uint(2) || !w.write_uint(frame.telemetry.rf_event.center_freq_hz)) return out;
    if (!w.write_uint(3) || !w.write_float(frame.telemetry.rf_event.features.avg_dbm)) return out;
    if (!w.write_uint(4) || !w.write_float(frame.telemetry.rf_event.features.peak_dbm)) return out;
    if (!w.write_uint(5) || !w.write_float(frame.telemetry.rf_event.anomaly_score)) return out;
    if (!w.write_uint(6) || !w.write_uint(frame.telemetry.rf_event.model_version)) return out;

    // GPS
    if (!w.write_uint(5) || !w.write_map_start(10)) return out;
    if (!w.write_uint(1) || !w.write_uint(frame.telemetry.gps.timestamp_ms)) return out;
    if (!w.write_uint(2) || !w.write_float(frame.telemetry.gps.latitude_deg)) return out;
    if (!w.write_uint(3) || !w.write_float(frame.telemetry.gps.longitude_deg)) return out;
    if (!w.write_uint(4) || !w.write_float(frame.telemetry.gps.altitude_m)) return out;
    if (!w.write_uint(5) || !w.write_uint(frame.telemetry.gps.num_sats)) return out;
    if (!w.write_uint(6) || !w.write_float(frame.telemetry.gps.hdop)) return out;
    if (!w.write_uint(7) || !w.write_uint(frame.telemetry.gps.valid_fix ? 1 : 0)) return out;
    if (!w.write_uint(8) || !w.write_uint(frame.telemetry.gps.jamming_detected ? 1 : 0)) return out;
    if (!w.write_uint(9) || !w.write_uint(frame.telemetry.gps.spoof_detected ? 1 : 0)) return out;
    if (!w.write_uint(10) || !w.write_float(frame.telemetry.gps.cn0_db_hz_avg)) return out;

    // Health
    if (!w.write_uint(6) || !w.write_map_start(5)) return out;
    if (!w.write_uint(1) || !w.write_uint(frame.telemetry.health.timestamp_ms)) return out;
    if (!w.write_uint(2) || !w.write_float(frame.telemetry.health.battery_v)) return out;
    if (!w.write_uint(3) || !w.write_float(frame.telemetry.health.temp_c)) return out;
    if (!w.write_uint(4) || !w.write_float(frame.telemetry.health.imu_tilt_deg)) return out;
    if (!w.write_uint(5) || !w.write_uint(frame.telemetry.health.tamper_flag ? 1 : 0)) return out;

    // Routing
    if (!w.write_uint(7)) return out;
    if (!encode_routing(w, frame.routing)) return out;

    // Fault
    if (!w.write_uint(8) || !w.write_map_start(4)) return out;
    if (!w.write_uint(1) || !w.write_uint(frame.fault.fault_active ? 1 : 0)) return out;
    if (!w.write_uint(2) || !w.write_uint(frame.fault.counters.watchdog_resets)) return out;
    if (!w.write_uint(3) || !w.write_uint(frame.fault.counters.ota_failures)) return out;
    if (!w.write_uint(4) || !w.write_uint(frame.fault.counters.tamper_events)) return out;

    // OTA
    if (!w.write_uint(9) || !w.write_map_start(4)) return out;
    if (!w.write_uint(1) || !w.write_uint(static_cast<uint8_t>(frame.ota.state))) return out;
    if (!w.write_uint(2) || !w.write_uint(frame.ota.current_offset)) return out;
    if (!w.write_uint(3) || !w.write_uint(frame.ota.total_size)) return out;
    if (!w.write_uint(4) || !w.write_uint(frame.ota.signature_valid ? 1 : 0)) return out;

    out.len = w.idx;
    return out;
}

static bool decode_mesh_frame_clear(const EncodedFrame& enc, MeshFrame& frame) {
    CborReader r{enc.bytes.data(), enc.len};
    std::size_t top_len = 0;
    if (!r.read_map_len(top_len)) return false;
    for (std::size_t i = 0; i < top_len; ++i) {
        uint32_t key = 0;
        if (!r.read_uint(key)) return false;
        switch (key) {
            case 1: { // header
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t hk = 0; r.read_uint(hk);
                    switch (hk) {
                        case 1: { uint32_t v = 0; r.read_uint(v); frame.header.version = static_cast<uint8_t>(v & 0xFF); break; }
                        case 2: { uint32_t mt = 0; r.read_uint(mt); frame.header.msg_type = static_cast<MeshMsgType>(mt); break; }
                        case 3: { uint32_t v = 0; r.read_uint(v); frame.header.ttl = static_cast<uint8_t>(v & 0xFF); break; }
                        case 4: { uint32_t v = 0; r.read_uint(v); frame.header.hop_count = static_cast<uint8_t>(v & 0xFF); break; }
                        case 5: r.read_uint(frame.header.seq_no); break;
                        case 6: r.read_text(frame.header.src_node_id, kMaxNodeIdLength); break;
                        case 7: r.read_text(frame.header.dest_node_id, kMaxNodeIdLength); break;
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            case 2: { // security
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t sk = 0; r.read_uint(sk);
                    switch (sk) {
                        case 1: { uint32_t encf = 0; r.read_uint(encf); frame.security.encrypted = encf != 0; break; }
                        case 2: {
                            std::size_t l = 0;
                            if (!r.read_bytes(frame.security.nonce.data(), frame.security.nonce.size(), l)) return false;
                            break;
                        }
                        case 3: {
                            std::size_t l = 0;
                            if (!r.read_bytes(frame.security.auth_tag.data(), frame.security.auth_tag.size(), l)) return false;
                            break;
                        }
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            case 3: { // counters
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t ck = 0; r.read_uint(ck);
                    switch (ck) {
                        case 1: r.read_uint(frame.counters.tx_counter); break;
                        case 2: r.read_uint(frame.counters.replay_window); break;
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            case 4: { // rf
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t rk = 0; r.read_uint(rk);
                    switch (rk) {
                        case 1: r.read_uint(frame.telemetry.rf_event.timestamp_ms); break;
                        case 2: r.read_uint(frame.telemetry.rf_event.center_freq_hz); break;
                        case 3: r.read_float(frame.telemetry.rf_event.features.avg_dbm); break;
                        case 4: r.read_float(frame.telemetry.rf_event.features.peak_dbm); break;
                        case 5: r.read_float(frame.telemetry.rf_event.anomaly_score); break;
                        case 6: { uint32_t mv = 0; r.read_uint(mv); frame.telemetry.rf_event.model_version = static_cast<uint8_t>(mv); break; }
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            case 5: { // gps
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t gk = 0; r.read_uint(gk);
                    switch (gk) {
                        case 1: r.read_uint(frame.telemetry.gps.timestamp_ms); break;
                        case 2: r.read_float(frame.telemetry.gps.latitude_deg); break;
                        case 3: r.read_float(frame.telemetry.gps.longitude_deg); break;
                        case 4: r.read_float(frame.telemetry.gps.altitude_m); break;
                        case 5: { uint32_t ns = 0; r.read_uint(ns); frame.telemetry.gps.num_sats = static_cast<uint8_t>(ns); break; }
                        case 6: r.read_float(frame.telemetry.gps.hdop); break;
                        case 7: { uint32_t vf = 0; r.read_uint(vf); frame.telemetry.gps.valid_fix = vf != 0; break; }
                        case 8: { uint32_t jm = 0; r.read_uint(jm); frame.telemetry.gps.jamming_detected = jm != 0; break; }
                        case 9: { uint32_t sp = 0; r.read_uint(sp); frame.telemetry.gps.spoof_detected = sp != 0; break; }
                        case 10: r.read_float(frame.telemetry.gps.cn0_db_hz_avg); break;
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            case 6: { // health
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t hk = 0; r.read_uint(hk);
                    switch (hk) {
                        case 1: r.read_uint(frame.telemetry.health.timestamp_ms); break;
                        case 2: r.read_float(frame.telemetry.health.battery_v); break;
                        case 3: r.read_float(frame.telemetry.health.temp_c); break;
                        case 4: r.read_float(frame.telemetry.health.imu_tilt_deg); break;
                        case 5: { uint32_t tf = 0; r.read_uint(tf); frame.telemetry.health.tamper_flag = tf != 0; break; }
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            case 7: // routing
                if (!decode_routing(r, frame.routing)) return false;
                break;
            case 8: { // fault
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t fk = 0; r.read_uint(fk);
                    switch (fk) {
                        case 1: { uint32_t fa = 0; r.read_uint(fa); frame.fault.fault_active = fa != 0; break; }
                        case 2: r.read_uint(frame.fault.counters.watchdog_resets); break;
                        case 3: r.read_uint(frame.fault.counters.ota_failures); break;
                        case 4: r.read_uint(frame.fault.counters.tamper_events); break;
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            case 9: { // ota
                std::size_t mlen = 0;
                if (!r.read_map_len(mlen)) return false;
                for (std::size_t j = 0; j < mlen; ++j) {
                    uint32_t ok = 0; r.read_uint(ok);
                    switch (ok) {
                        case 1: { uint32_t st = 0; r.read_uint(st); frame.ota.state = static_cast<OtaState>(st); break; }
                        case 2: r.read_uint(frame.ota.current_offset); break;
                        case 3: r.read_uint(frame.ota.total_size); break;
                        case 4: { uint32_t sv = 0; r.read_uint(sv); frame.ota.signature_valid = sv != 0; break; }
                        default: r.skip_value(); break;
                    }
                }
                break;
            }
            default:
                r.skip_value();
                break;
        }
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
