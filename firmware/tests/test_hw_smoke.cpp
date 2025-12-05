#include "adc.hpp"
#include "sensors.hpp"
#include "mesh.hpp"
#include "mesh_encode.hpp"
#include "radio_driver.hpp"
#include <cstdio>

int main() {
#ifndef ESP_PLATFORM
    std::printf("hw smoke skipped: not on ESP_PLATFORM\n");
    return 0;
#else
    init_adc();
    init_sensors();
    init_mesh();
    init_radio_driver();

    auto window = collect_rf_window(0);
    if (window.sample_count == 0) {
        std::printf("hw smoke: ADC returned zero samples\n");
        return 1;
    }

    auto gps = read_gps_status(0);
    std::printf("hw smoke: gps valid=%d sats=%u lat=%.5f lon=%.5f\n",
        gps.valid_fix ? 1 : 0,
        gps.num_sats,
        gps.latitude_deg,
        gps.longitude_deg);

    MeshFrame frame{};
    frame.header.version = 1;
    frame.header.msg_type = MeshMsgType::Telemetry;
    frame.header.ttl = 2;
    frame.header.seq_no = 1;
    std::snprintf(frame.header.src_node_id, sizeof(frame.header.src_node_id), "smoke");

    EncryptedFrame dummy{};
    dummy.len = 32;
    dummy.bytes.fill(0xAA);
    const bool sent = radio_driver_send(dummy);
    std::printf("hw smoke: radio send returned %d\n", sent ? 1 : 0);
    // Do not fail test on radio send failures; they may occur if radio not provisioned.
    return 0;
#endif
}
