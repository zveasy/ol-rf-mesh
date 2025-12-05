#include "mesh.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#ifdef ESP_PLATFORM
#include "radio_driver.hpp"
#endif

namespace {
MeshRoutingPayload g_routing{};
MeshSendHandler g_send_handler = nullptr;
char g_self_id[kMaxNodeIdLength]{};
uint32_t g_routing_version = 0;
MeshMetrics g_metrics{};

struct SeenFrame {
    char src[kMaxNodeIdLength];
    uint32_t seq;
};

constexpr std::size_t kSeenWindow = 8;
std::array<SeenFrame, kSeenWindow> g_seen{};

struct BlacklistEntry {
    char neighbor_id[kMaxNodeIdLength];
    uint8_t strikes;
};
constexpr std::size_t kMaxBlacklist = 4;
std::array<BlacklistEntry, kMaxBlacklist> g_blacklist{};

void add_route_internal(const RouteEntry& entry) {
    if (g_routing.entry_count >= kMaxRoutes) {
        return;
    }
    g_routing.entries[g_routing.entry_count++] = entry;
    g_routing.version = ++g_routing_version;
}

bool seen_before(const MeshFrame& frame) {
    for (auto& s : g_seen) {
        if (s.src[0] == '\0') {
            std::snprintf(s.src, sizeof(s.src), "%s", frame.header.src_node_id);
            s.seq = frame.header.seq_no;
            return false;
        }
        if (std::strncmp(s.src, frame.header.src_node_id, sizeof(s.src)) == 0) {
            if (frame.header.seq_no <= s.seq) {
                return true;
            }
            s.seq = frame.header.seq_no;
            return false;
        }
    }
    // Overwrite oldest slot 0
    std::snprintf(g_seen[0].src, sizeof(g_seen[0].src), "%s", frame.header.src_node_id);
    g_seen[0].seq = frame.header.seq_no;
    return false;
}

bool is_blacklisted_internal(const char* neighbor_id) {
    for (const auto& b : g_blacklist) {
        if (b.neighbor_id[0] == '\0') continue;
        if (std::strncmp(b.neighbor_id, neighbor_id, kMaxNodeIdLength) == 0 && b.strikes > 0) {
            return true;
        }
    }
    return false;
}

void prune_and_sort_routes() {
    // Remove blacklisted entries and sort by link quality descending, cost ascending.
    MeshRoutingPayload filtered{};
    filtered.epoch_ms = g_routing.epoch_ms;
    filtered.version = g_routing.version;

    for (std::size_t i = 0; i < g_routing.entry_count; ++i) {
        const auto& e = g_routing.entries[i];
        if (is_blacklisted_internal(e.neighbor_id)) continue;
        if (filtered.entry_count < kMaxRoutes) {
            filtered.entries[filtered.entry_count++] = e;
        }
    }
    std::sort(filtered.entries.begin(), filtered.entries.begin() + filtered.entry_count, [](const RouteEntry& a, const RouteEntry& b) {
        if (a.link_quality == b.link_quality) {
            return a.cost < b.cost;
        }
        return a.link_quality > b.link_quality;
    });
    g_routing = filtered;
}
} // namespace

void init_mesh() {
    g_routing.entry_count = 0;
    g_routing.version = 0;
    g_routing.epoch_ms = 0;
    g_routing_version = 0;
    std::memset(g_self_id, 0, sizeof(g_self_id));
    for (auto& s : g_seen) {
        s.src[0] = '\0';
        s.seq = 0;
    }
    for (auto& b : g_blacklist) {
        b.neighbor_id[0] = '\0';
        b.strikes = 0;
    }
    g_metrics = {};
}

void set_mesh_node_id(const char* node_id) {
    if (node_id) {
        std::snprintf(g_self_id, sizeof(g_self_id), "%s", node_id);
    }
}

void set_mesh_send_handler(MeshSendHandler handler) {
    g_send_handler = handler;
}

bool send_mesh_frame(const MeshFrame& frame) {
    AesGcmKey key{};
    key.bytes.fill(0x11); // placeholder; real key should be loaded from cfg

    if (frame.header.ttl == 0 || frame.header.hop_count >= frame.header.ttl) {
        g_metrics.ttl_drops++;
        return false;
    }

    const EncryptedFrame encoded = encrypt_mesh_frame(frame, key);
    // Simple fragmentation guard: drop if we exceed 3 fragments worth of a 200-byte MTU.
    constexpr std::size_t kLinkMtu = 200;
    const std::size_t needed_frags = (encoded.len + kLinkMtu - 1) / kLinkMtu;
    if (needed_frags > 3) {
        g_metrics.fragments_dropped++;
        return false;
    }
    if (needed_frags > 1) {
        g_metrics.fragments_sent += static_cast<uint32_t>(needed_frags);
    }
    std::printf(
        "[MESH] seq=%u ttl=%u hop=%u type=%u len=%zu rf_peak=%.2f gps_valid=%d battery=%.2f routes=%zu\n",
        static_cast<unsigned>(frame.header.seq_no),
        frame.header.ttl,
        frame.header.hop_count,
        static_cast<uint8_t>(frame.header.msg_type),
        encoded.len,
        frame.telemetry.rf_event.features.peak_dbm,
        frame.telemetry.gps.valid_fix ? 1 : 0,
        frame.telemetry.health.battery_v,
        frame.routing.entry_count
    );
    if (g_send_handler) {
        return g_send_handler(encoded);
    }
#ifdef ESP_PLATFORM
    return radio_driver_send(encoded);
#else
    return true;
#endif
}

void add_route_entry(const RouteEntry& entry) {
    // Replace existing entry if neighbor matches.
    for (std::size_t i = 0; i < g_routing.entry_count; ++i) {
        if (std::strncmp(g_routing.entries[i].neighbor_id, entry.neighbor_id, kMaxNodeIdLength) == 0) {
            g_routing.entries[i] = entry;
            g_routing.version = ++g_routing_version;
            prune_and_sort_routes();
            return;
        }
    }
    add_route_internal(entry);
    prune_and_sort_routes();
}

MeshRoutingPayload current_routing_payload() {
    return g_routing;
}

bool ingest_route_update(const MeshRoutingPayload& from_neighbor, const char* neighbor_id, uint8_t link_quality, int8_t rssi_dbm) {
    if (neighbor_id == nullptr || neighbor_id[0] == '\0') {
        return false;
    }
    bool changed = false;
    RouteEntry self_to_neighbor{};
    std::snprintf(self_to_neighbor.neighbor_id, sizeof(self_to_neighbor.neighbor_id), "%s", neighbor_id);
    self_to_neighbor.rssi_dbm = rssi_dbm;
    self_to_neighbor.link_quality = link_quality;
    self_to_neighbor.cost = 1;
    const std::size_t prev_version = g_routing_version;
    add_route_entry(self_to_neighbor);
    changed = changed || (g_routing_version != prev_version);

    // Merge neighbor's routes with +1 cost
    for (std::size_t i = 0; i < from_neighbor.entry_count; ++i) {
        const RouteEntry& n = from_neighbor.entries[i];
        if (std::strncmp(n.neighbor_id, g_self_id, kMaxNodeIdLength) == 0) {
            continue; // avoid loops to self
        }
        RouteEntry candidate = n;
        candidate.cost = static_cast<uint8_t>(std::min<uint16_t>(n.cost + 1, 255));
        candidate.link_quality = std::min<uint16_t>(n.link_quality, static_cast<uint16_t>(link_quality));
        add_route_entry(candidate);
    }
    return changed;
}

RouteEntry select_best_parent() {
    RouteEntry best{};
    RouteEntry prev_best{};
    if (g_routing.entry_count > 0) {
        prev_best = g_routing.entries[0];
    }
    if (g_routing.entry_count == 0) {
        return best;
    }
    prune_and_sort_routes();
    if (g_routing.entry_count > 0) {
        best = g_routing.entries[0];
    }
    if (std::strncmp(prev_best.neighbor_id, best.neighbor_id, kMaxNodeIdLength) != 0 && best.neighbor_id[0] != '\0') {
        g_metrics.parent_changes++;
    }
    return best;
}

void blacklist_route(const char* neighbor_id) {
    if (!neighbor_id) return;
    for (auto& b : g_blacklist) {
        if (b.neighbor_id[0] == '\0') {
            std::snprintf(b.neighbor_id, sizeof(b.neighbor_id), "%s", neighbor_id);
            b.strikes = 1;
            g_metrics.blacklist_hits++;
            prune_and_sort_routes();
            return;
        }
        if (std::strncmp(b.neighbor_id, neighbor_id, kMaxNodeIdLength) == 0) {
            b.strikes = static_cast<uint8_t>(std::min<uint16_t>(b.strikes + 1, 255));
            g_metrics.blacklist_hits++;
            prune_and_sort_routes();
            return;
        }
    }
    // Overwrite first slot
    std::snprintf(g_blacklist[0].neighbor_id, sizeof(g_blacklist[0].neighbor_id), "%s", neighbor_id);
    g_blacklist[0].strikes = 1;
    g_metrics.blacklist_hits++;
    prune_and_sort_routes();
}

bool is_route_blacklisted(const char* neighbor_id) {
    return is_blacklisted_internal(neighbor_id);
}

bool should_forward_frame(MeshFrame& frame) {
    if (frame.header.ttl == 0 || frame.header.hop_count >= frame.header.ttl) {
        g_metrics.ttl_drops++;
        return false;
    }
    if (seen_before(frame)) {
        return false;
    }
    frame.header.hop_count = static_cast<uint8_t>(frame.header.hop_count + 1);
    return true;
}

MeshMetrics mesh_metrics() {
    return g_metrics;
}

void reset_mesh_metrics() {
    g_metrics = {};
}

void note_retry_drop() {
    g_metrics.retry_drops++;
}
