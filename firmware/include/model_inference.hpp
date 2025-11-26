#pragma once

#include "sensors.hpp"

void init_model_inference();
float run_model_inference(const SensorReadings& readings);
