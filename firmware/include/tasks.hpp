#pragma once

#include "config.hpp"
#include "telemetry.hpp"
#include "fault.hpp"
#include "ota.hpp"

struct TaskStatus {
    TaskHeartbeat rf_scan;
    TaskHeartbeat fft;
    TaskHeartbeat gnss;
    TaskHeartbeat health;
    TaskHeartbeat packet_builder;
    TaskHeartbeat ota;
    TaskHeartbeat fault_monitor;
    FaultStatus faults;
};

TaskStatus run_firmware_cycle(const NodeConfig& cfg, uint32_t now_ms);
