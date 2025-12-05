#pragma once

#include <cstdint>

// Lightweight abstraction to allow host builds to compile while enabling
// hardware watchdog wiring on MCU targets (e.g., ESP-IDF task WDT).
// timeout_ms of 0 lets the platform choose a default.
void watchdog_init(uint32_t timeout_ms);
void watchdog_register_task(const char* name, uint32_t timeout_ms);
void watchdog_feed(const char* name);
