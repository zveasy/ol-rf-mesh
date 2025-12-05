#pragma once

#include "telemetry.hpp"
#include "mesh_encode.hpp"
#include "crypto.hpp"

void init_mesh();
void set_mesh_node_id(const char* node_id);
using MeshSendHandler = bool(*)(const EncryptedFrame&);
void set_mesh_send_handler(MeshSendHandler handler);
bool send_mesh_frame(const MeshFrame& frame);
void add_route_entry(const RouteEntry& entry);
MeshRoutingPayload current_routing_payload();

// Routing helpers
bool ingest_route_update(const MeshRoutingPayload& from_neighbor, const char* neighbor_id, uint8_t link_quality, int8_t rssi_dbm);
RouteEntry select_best_parent();
void blacklist_route(const char* neighbor_id);
bool is_route_blacklisted(const char* neighbor_id);
bool should_forward_frame(MeshFrame& frame);

struct MeshMetrics {
    uint32_t parent_changes;
    uint32_t blacklist_hits;
    uint32_t ttl_drops;
    uint32_t fragments_sent;
    uint32_t fragments_dropped;
    uint32_t retry_drops;
};

MeshMetrics mesh_metrics();
void reset_mesh_metrics();
void note_retry_drop();
