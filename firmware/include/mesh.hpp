#pragma once

#include <cstdint>

struct MeshPacket {
    uint32_t timestamp;
    float rf_power_dbm;
    float battery_v;
    float temp_c;
    float anomaly_score;
    char node_id[16];
};

void init_mesh();
void send_mesh_packet(const MeshPacket& pkt);
