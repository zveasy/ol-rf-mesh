#include "mesh.hpp"
#include "telemetry.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <cstdio>

int main() {
    // Start from a clean routing state.
    init_mesh();

    MeshRoutingPayload payload = current_routing_payload();
    assert(payload.entry_count == 0);

    // Add a single route entry and verify it appears.
    RouteEntry e1{};
    std::strncpy(e1.neighbor_id, "node-A", kMaxNodeIdLength);
    e1.rssi_dbm = -60;
    e1.link_quality = 200;
    e1.cost = 1;

    add_route_entry(e1);

    payload = current_routing_payload();
    assert(payload.entry_count == 1);
    assert(std::string(payload.entries[0].neighbor_id) == std::string("node-A"));
    assert(payload.entries[0].rssi_dbm == -60);
    assert(payload.entries[0].link_quality == 200);
    assert(payload.entries[0].cost == 1);

    // Fill up to kMaxRoutes and ensure we never exceed capacity.
    for (std::size_t i = 1; i <= kMaxRoutes; ++i) {
        RouteEntry e{};
        std::snprintf(e.neighbor_id, kMaxNodeIdLength, "node-%zu", i);
        e.rssi_dbm = -50;
        e.link_quality = static_cast<uint8_t>(100 + i);
        e.cost = static_cast<uint8_t>(i);
        add_route_entry(e);
    }

    payload = current_routing_payload();
    assert(payload.entry_count <= kMaxRoutes);

    std::cout << "OK test_mesh_routing: entry_count=" << payload.entry_count << " (max=" << kMaxRoutes << ")\n";
    return 0;
}
