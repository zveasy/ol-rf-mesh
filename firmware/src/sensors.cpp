#include "sensors.hpp"

void init_sensors() {
}

HealthStatus read_health_status(uint32_t now_ms) {
    HealthStatus health{};
    health.timestamp_ms = now_ms;
    health.battery_v = 3.7f;
    health.temp_c = 25.0f;
    health.imu_tilt_deg = 0.5f;
    health.tamper_flag = false;
    return health;
}

GpsStatus read_gps_status(uint32_t now_ms) {
    GpsStatus gps{};
    gps.timestamp_ms = now_ms;
    gps.latitude_deg = 37.7749f;
    gps.longitude_deg = -122.4194f;
    gps.altitude_m = 10.0f;
    gps.num_sats = 7;
    gps.hdop = 1.2f;
    gps.valid_fix = true;
    gps.jamming_detected = false;
    gps.spoof_detected = false;
    gps.cn0_db_hz_avg = 38.0f;
    return gps;
}
