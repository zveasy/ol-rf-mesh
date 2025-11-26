#pragma once

#include "telemetry.hpp"

void init_model_inference();
RfFeatures extract_rf_features(const RFSampleWindow& window);
float run_model_inference(const RfFeatures& features);
