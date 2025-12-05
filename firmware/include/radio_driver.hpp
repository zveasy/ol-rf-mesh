#pragma once

#include "mesh_encode.hpp"

enum class RadioTransport {
    EspNow,
    WifiRaw,
    LoRa,
};

// Platform radio abstraction. On IDF, this can wrap ESP-NOW/Wi-Fi/LoRa.
// Host builds stub out the send path but still wire the transport queue.
void init_radio_driver();
void set_radio_transport(RadioTransport mode);
RadioTransport current_radio_transport();
bool radio_driver_send(const EncryptedFrame& frame);
