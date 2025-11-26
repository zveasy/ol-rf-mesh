#include "mesh.hpp"
#include <cstdio>

namespace {
MeshRoutingPayload g_routing{};

void add_route_internal(const RouteEntry& entry) {
    if (g_routing.entry_count >= kMaxRoutes) {
        return;
    }
    g_routing.entries[g_routing.entry_count++] = entry;
}
} // namespace

void init_mesh() {
    g_routing.entry_count = 0;
}

void send_mesh_frame(const MeshFrame& frame) {
    AesGcmKey key{};
    key.bytes.fill(0x11); // placeholder; real key should be loaded from cfg

    const EncryptedFrame encoded = encrypt_mesh_frame(frame, key);
    std::printf(
        "[MESH] seq=%u ttl=%u hop=%u type=%u len=%zu rf_peak=%.2f gps_valid=%d battery=%.2f routes=%zu\n",
        frame.header.seq_no,
        frame.header.ttl,
        frame.header.hop_count,
        static_cast<uint8_t>(frame.header.msg_type),
        encoded.len,
        frame.telemetry.rf_event.features.peak_dbm,
        frame.telemetry.gps.valid_fix ? 1 : 0,
        frame.telemetry.health.battery_v,
        frame.routing.entry_count
    );
}

void add_route_entry(const RouteEntry& entry) {
    add_route_internal(entry);
}

MeshRoutingPayload current_routing_payload() {
    return g_routing;
}
