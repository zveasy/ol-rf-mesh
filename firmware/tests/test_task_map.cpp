#include "tasks.hpp"
#include "config.hpp"

#include <array>
#include <cassert>
#include <string>

int main() {
    const auto& plan = task_plan();
    static const std::array<const char*, kTaskCount> expected_names{{
        "FaultMonitorTask",
        "RFScanTask",
        "FFTTflmTask",
        "PacketBuilderTask",
        "TransportTask",
        "GNSSMonitorTask",
        "SensorHealthTask",
        "OtaUpdateTask",
    }};
    static const std::array<uint8_t, kTaskCount> expected_priorities{{6, 5, 5, 4, 4, 3, 3, 2}};
    static const std::array<uint16_t, kTaskCount> expected_stacks{{768, 2048, 3584, 2048, 2048, 1536, 1536, 2048}};
    static const std::array<uint32_t, kTaskCount> expected_periods{{250, 500, 500, 1000, 250, 2000, 1000, 5000}};
    static const std::array<uint32_t, kTaskCount> expected_watchdogs{{750, 1000, 1000, 2000, 750, 0, 2000, 8000}};

    int failures = 0;
    for (std::size_t i = 0; i < kTaskCount; ++i) {
        if (std::string(plan[i].name) != std::string(expected_names[i])) failures++;
        if (plan[i].priority != expected_priorities[i]) failures++;
        if (plan[i].stack_words != expected_stacks[i]) failures++;
        if (plan[i].period_ms != expected_periods[i]) failures++;
        if (plan[i].watchdog_protected != (expected_watchdogs[i] > 0)) failures++;
        if (plan[i].watchdog_protected && plan[i].watchdog_budget_ms != expected_watchdogs[i]) failures++;
    }
    if (failures != 0) {
        return 1;
    }

    NodeConfig cfg = load_config();
    uint32_t now_ms = 0;
    TaskStatus status{};
    for (int i = 0; i < 48; ++i) {
        status = run_firmware_cycle(cfg, now_ms);
        now_ms += 250;
    }

    assert(status.transport.last_beat_ms > 0);
    assert(status.rf_scan.last_beat_ms > 0);
    assert(status.fft.last_beat_ms > 0);
    assert(status.packet_builder.last_beat_ms > 0);
    assert(status.health.last_beat_ms > 0);
    assert(status.faults.counters.watchdog_resets == 0);
    assert(!status.faults.fault_active);

    return 0;
}
