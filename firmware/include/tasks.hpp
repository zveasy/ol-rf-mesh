#pragma once

#include <array>
#include "config.hpp"
#include "telemetry.hpp"
#include "fault.hpp"
#include "ota.hpp"
#include <cstddef>

struct TaskConfig {
    const char* name;
    uint8_t priority;
    uint16_t stack_words;
    uint32_t period_ms;
    bool watchdog_protected;
    uint32_t watchdog_budget_ms;
};

struct TaskStatus {
    TaskHeartbeat transport;
    TaskHeartbeat rf_scan;
    TaskHeartbeat fft;
    TaskHeartbeat gnss;
    TaskHeartbeat health;
    TaskHeartbeat packet_builder;
    TaskHeartbeat ota;
    TaskHeartbeat fault_monitor;
    FaultStatus faults;
};

constexpr std::size_t kTaskCount = 8;

const std::array<TaskConfig, kTaskCount>& task_plan();
TaskStatus run_firmware_cycle(const NodeConfig& cfg, uint32_t now_ms);
void start_freertos_tasks(const NodeConfig& cfg);
