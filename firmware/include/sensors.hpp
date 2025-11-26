#pragma once

struct SensorReadings {
    float rf_power_dbm;
    float battery_v;
    float temp_c;
};

void init_sensors();
SensorReadings read_sensors();
