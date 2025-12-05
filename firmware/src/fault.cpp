#include "fault.hpp"

namespace {
FaultStatus g_fault{
    false,
    nullptr,
    {0, 0, 0}
};
} // namespace

void init_fault_monitor() {
    g_fault.fault_active = false;
    g_fault.fault_msg = nullptr;
}

void record_fault(const char* msg) {
    g_fault.fault_active = true;
    g_fault.fault_msg = msg;
}

void record_ota_failure() {
    g_fault.counters.ota_failures += 1;
    record_fault("OTA failure");
}

void record_tamper() {
    g_fault.counters.tamper_events += 1;
    record_fault("Tamper detected");
}

void record_watchdog_reset() {
    g_fault.counters.watchdog_resets += 1;
    record_fault("Watchdog reset");
}

FaultStatus fault_status() {
    return g_fault;
}
