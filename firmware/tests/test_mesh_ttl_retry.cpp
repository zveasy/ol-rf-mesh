#include "mesh.hpp"
#include "telemetry.hpp"

#include <cassert>
#include <string>

int main() {
    init_mesh();
    reset_mesh_metrics();
    set_mesh_node_id("self");

    MeshFrame f{};
    f.header.version = 1;
    f.header.msg_type = MeshMsgType::Telemetry;
    f.header.ttl = 1;
    f.header.hop_count = 1; // at limit; should drop
    f.header.seq_no = 1;
    std::snprintf(f.header.src_node_id, sizeof(f.header.src_node_id), "node-x");
    std::snprintf(f.header.dest_node_id, sizeof(f.header.dest_node_id), "gw");
    f.telemetry.gps.valid_fix = true;

    bool ok = send_mesh_frame(f);
    if (ok) {
        return 1;
    }
    MeshMetrics m = mesh_metrics();
    assert(m.ttl_drops >= 1);

    note_retry_drop();
    m = mesh_metrics();
    assert(m.retry_drops >= 1);

    return 0;
}
