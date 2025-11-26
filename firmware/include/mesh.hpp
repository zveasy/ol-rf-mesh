#pragma once

#include "telemetry.hpp"
#include "mesh_encode.hpp"
#include "crypto.hpp"

void init_mesh();
void send_mesh_frame(const MeshFrame& frame);
void add_route_entry(const RouteEntry& entry);
MeshRoutingPayload current_routing_payload();
