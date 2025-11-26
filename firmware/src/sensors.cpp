#include "sensors.hpp"

void init_sensors() {
}

SensorReadings read_sensors() {
    SensorReadings r{};
    r.rf_power_dbm = -30.0f;
    r.battery_v = 3.7f;
    r.temp_c = 25.0f;
    return r;
}
