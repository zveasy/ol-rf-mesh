#pragma once

#include <cstdint>

struct FaultCounters {
    uint32_t watchdog_resets;
    uint32_t ota_failures;
    uint32_t tamper_events;
};

struct FaultStatus {
    bool fault_active;
    const char* fault_msg;
    FaultCounters counters;
};

void init_fault_monitor();
void record_fault(const char* msg);
void record_ota_failure();
void record_tamper();
void record_watchdog_reset();
FaultStatus fault_status();
