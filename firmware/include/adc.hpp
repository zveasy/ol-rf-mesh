#pragma once

#include "telemetry.hpp"

void init_adc();

RFSampleWindow collect_rf_window(uint32_t now_ms);
