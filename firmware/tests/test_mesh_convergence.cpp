#include "mesh.hpp"
#include "telemetry.hpp"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

static RouteEntry make_entry(const char* id, int8_t rssi, uint8_t lq, uint8_t cost) {
    RouteEntry e{};
    std::snprintf(e.neighbor_id, sizeof(e.neighbor_id), "%s", id);
    e.rssi_dbm = rssi;
    e.link_quality = lq;
    e.cost = cost;
    return e;
}

int main() {
    init_mesh();
    set_mesh_node_id("self");

    // Seed a couple of routes and ensure version increments.
    MeshRoutingPayload p{};
    p.entry_count = 2;
    p.entries[0] = make_entry("node-A", -60, 180, 1);
    p.entries[1] = make_entry("node-B", -70, 120, 2);
    bool changed = ingest_route_update(p, "node-A", 180, -60);
    assert(changed);
    MeshRoutingPayload cur = current_routing_payload();
    assert(cur.entry_count >= 1);
    const uint32_t v1 = cur.version;

    // Ingest same payload again should not meaningfully change version after dedupe.
    changed = ingest_route_update(p, "node-A", 180, -60);
    cur = current_routing_payload();
    if (cur.version < v1) {
        return 1;
    }
    (void)changed; // version should not regress

    // Add a better parent and ensure selection prefers link_quality.
    MeshRoutingPayload p2{};
    p2.entry_count = 1;
    p2.entries[0] = make_entry("node-C", -50, 200, 1);
    ingest_route_update(p2, "node-C", 200, -50);
    RouteEntry parent = select_best_parent();
    assert(std::string(parent.neighbor_id) == "node-C");

    // Blacklist node-C and ensure parent switches.
    blacklist_route("node-C");
    parent = select_best_parent();
    assert(std::string(parent.neighbor_id) != "node-C");

    // Loop/TTL guard: hop_count should increment and drop when exceeded.
    MeshFrame f{};
    f.header.ttl = 2;
    f.header.hop_count = 1;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "node-X");
    f.header.seq_no = 1;
    bool forward_ok = should_forward_frame(f);
    if (!forward_ok) {
        return 1;
    }
    assert(f.header.hop_count == 2);

    // Next attempt with same seq should be dropped as loop/seen.
    f.header.hop_count = 1;
    forward_ok = should_forward_frame(f);
    if (forward_ok) {
        return 1;
    }

    // Soak: repeatedly merge random-ish routes and ensure capacity respected.
    for (int i = 0; i < 20; ++i) {
        MeshRoutingPayload dyn{};
        dyn.entry_count = 2;
        char nid1[8];
        std::snprintf(nid1, sizeof(nid1), "N%02d", i);
        dyn.entries[0] = make_entry(nid1, -50, static_cast<uint8_t>(150 + (i % 40)), 1);
        dyn.entries[1] = make_entry("node-A", -60, 180, 1);
        ingest_route_update(dyn, nid1, dyn.entries[0].link_quality, dyn.entries[0].rssi_dbm);
        cur = current_routing_payload();
        assert(cur.entry_count <= kMaxRoutes);
    }

    return 0;
}
