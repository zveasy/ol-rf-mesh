#pragma once

#include "telemetry.hpp"

void init_sensors();

HealthStatus read_health_status(uint32_t now_ms);
GpsStatus read_gps_status(uint32_t now_ms);
