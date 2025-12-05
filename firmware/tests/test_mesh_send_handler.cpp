#include "mesh.hpp"
#include "telemetry.hpp"

#include <cassert>
#include <cstdio>

static bool g_called = false;

static bool failing_sender(const EncryptedFrame&) {
    g_called = true;
    return false;
}

int main() {
    init_mesh();
    set_mesh_send_handler(failing_sender);

    MeshFrame frame{};
    frame.header.version = 1;
    frame.header.msg_type = MeshMsgType::Telemetry;
    frame.header.ttl = 3;
    frame.header.hop_count = 0;
    frame.header.seq_no = 1;
    std::snprintf(frame.header.src_node_id, sizeof(frame.header.src_node_id), "node-A");
    std::snprintf(frame.header.dest_node_id, sizeof(frame.header.dest_node_id), "gw");

    frame.telemetry.rf_event.features.avg_dbm = -60.0f;
    frame.telemetry.rf_event.features.peak_dbm = -40.0f;
    frame.telemetry.rf_event.model_version = 1;
    frame.telemetry.gps.valid_fix = true;

    bool ok = send_mesh_frame(frame);
    (void)ok;
    assert(!ok);
    assert(g_called);
    return 0;
}
