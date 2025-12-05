#include "config.hpp"
#include "logging.hpp"
#include "spi_bus.hpp"
#include "adc.hpp"
#include "sensors.hpp"
#include "mesh.hpp"
#include "model_inference.hpp"
#include "tasks.hpp"
#include "ota.hpp"
#include "fault.hpp"
#include "radio_driver.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
    init_logging();
    log_info("O&L RF Node (IDF) booting...");

    NodeConfig cfg = load_config();

    init_spi_bus();
    init_adc();
    init_sensors();
    init_mesh();
    set_mesh_node_id(cfg.node_id.c_str());
    init_radio_driver();
    init_model_inference();
    init_ota();
    init_fault_monitor();

    RouteEntry gw{};
    std::snprintf(gw.neighbor_id, sizeof(gw.neighbor_id), "gateway");
    gw.rssi_dbm = -55;
    gw.link_quality = 90;
    gw.cost = 1;
    add_route_entry(gw);

    start_freertos_tasks(cfg);

    // Sleep forever while tasks run.
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}
