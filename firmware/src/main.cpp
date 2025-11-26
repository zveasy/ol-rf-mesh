#include <cstdio>
#include "config.hpp"
#include "logging.hpp"
#include "spi_bus.hpp"
#include "adc.hpp"
#include "sensors.hpp"
#include "mesh.hpp"
#include "model_inference.hpp"

static void delay_ms(unsigned int /*ms*/) {
    // Simple stub for now
}

int main() {
    init_logging();
    log_info("O&L RF Node booting...");

    init_spi_bus();
    init_adc();
    init_sensors();
    init_mesh();
    init_model_inference();

    while (true) {
        SensorReadings readings = read_sensors();
        float anomaly_score = run_model_inference(readings);

        MeshPacket pkt{0, readings.rf_power_dbm, readings.battery_v, readings.temp_c, anomaly_score, {0}};
        send_mesh_packet(pkt);

        delay_ms(1000);
        break; // avoid infinite loop in native stub
    }

    return 0;
}
