#include "mesh.hpp"
#include "telemetry.hpp"

#include <cassert>
#include <string>

static MeshRoutingPayload make_payload(const char* nid, uint8_t lq, int8_t rssi, uint8_t cost) {
    MeshRoutingPayload p{};
    p.entry_count = 1;
    std::snprintf(p.entries[0].neighbor_id, sizeof(p.entries[0].neighbor_id), "%s", nid);
    p.entries[0].link_quality = lq;
    p.entries[0].rssi_dbm = rssi;
    p.entries[0].cost = cost;
    return p;
}

int main() {
    init_mesh();
    reset_mesh_metrics();
    set_mesh_node_id("self");

    // Prefer parent with higher link quality.
    auto pA = make_payload("A", 180, -60, 1);
    ingest_route_update(pA, "A", 180, -60);
    RouteEntry parent = select_best_parent();
    assert(std::string(parent.neighbor_id) == "A");

    auto pB = make_payload("B", 200, -55, 1);
    ingest_route_update(pB, "B", 200, -55);
    parent = select_best_parent();
    assert(std::string(parent.neighbor_id) == "B");

    MeshMetrics m = mesh_metrics();
    assert(m.parent_changes >= 1);

    // Blacklist current parent and ensure it is avoided.
    blacklist_route("B");
    parent = select_best_parent();
    assert(std::string(parent.neighbor_id) != "B");
    m = mesh_metrics();
    assert(m.blacklist_hits >= 1);

    // Ensure TTL guard works on forward path.
    MeshFrame f{};
    f.header.ttl = 1;
    f.header.hop_count = 1;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "src");
    f.header.seq_no = 42;
    const bool ok = should_forward_frame(f);
    if (ok) {
        return 1;
    }
    m = mesh_metrics();
    assert(m.ttl_drops >= 1);

    return 0;
}
