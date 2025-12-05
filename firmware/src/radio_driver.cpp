#include "radio_driver.hpp"
#include "mesh.hpp"

#ifdef ESP_PLATFORM
#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"
#endif

namespace {
RadioTransport g_transport_mode = RadioTransport::EspNow;

#ifdef ESP_PLATFORM
constexpr std::size_t kEspNowMaxPayload = 250; // ESP-NOW per-packet payload limit
const uint8_t kBroadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool g_radio_ready = false;

bool check_esp(esp_err_t err, const char* msg) {
    if (err != ESP_OK) {
        ESP_LOGE("RADIO", "%s failed: %s", msg, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool send_espnow(const EncryptedFrame& frame) {
    if (!g_radio_ready) {
        ESP_LOGW("RADIO", "send dropped: radio not ready");
        return false;
    }
    if (frame.len == 0 || frame.len > kEspNowMaxPayload) {
        ESP_LOGW("RADIO", "send dropped: len=%zu exceeds ESP-NOW limit %zu", frame.len, kEspNowMaxPayload);
        return false;
    }
    const esp_err_t err = esp_now_send(kBroadcastAddr, frame.bytes.data(), frame.len);
    if (err != ESP_OK) {
        ESP_LOGW("RADIO", "esp_now_send failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool send_wifi_raw(const EncryptedFrame& frame) {
    if (!g_radio_ready) {
        ESP_LOGW("RADIO", "wifi raw send dropped: radio not ready");
        return false;
    }
    if (frame.len == 0 || frame.len > kMaxCipherLen) {
        ESP_LOGW("RADIO", "wifi raw send dropped: invalid len=%zu", frame.len);
        return false;
    }
    const esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, frame.bytes.data(), frame.len, false);
    if (err != ESP_OK) {
        ESP_LOGW("RADIO", "esp_wifi_80211_tx failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool send_lora(const EncryptedFrame& frame) {
#ifdef CONFIG_OL_LORA_SUPPORTED
    // Placeholder: integrate with actual LoRa driver.
    // Return false to signal retry if not wired.
    (void)frame;
    ESP_LOGW("RADIO", "LoRa path selected but not implemented");
    return false;
#else
    (void)frame;
    ESP_LOGW("RADIO", "LoRa transport not compiled in");
    return false;
#endif
}

bool driver_send(const EncryptedFrame& frame) {
    switch (g_transport_mode) {
        case RadioTransport::WifiRaw:
            return send_wifi_raw(frame);
        case RadioTransport::LoRa:
            return send_lora(frame);
        case RadioTransport::EspNow:
        default:
            return send_espnow(frame);
    }
}

bool init_espnow() {
    if (!check_esp(esp_netif_init(), "esp_netif_init")) return false;
    if (!check_esp(esp_event_loop_create_default(), "esp_event_loop_create_default")) return false;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (!check_esp(esp_wifi_init(&cfg), "esp_wifi_init")) return false;
    if (!check_esp(esp_wifi_set_storage(WIFI_STORAGE_RAM), "esp_wifi_set_storage")) return false;
    if (!check_esp(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode")) return false;
    if (!check_esp(esp_wifi_start(), "esp_wifi_start")) return false;

    if (!check_esp(esp_now_init(), "esp_now_init")) return false;
    g_radio_ready = true;
    ESP_LOGI("RADIO", "ESP-NOW ready");
    return true;
}
#else
bool driver_send(const EncryptedFrame& frame) {
    (void)frame;
    return true;
}
#endif
} // namespace

void init_radio_driver() {
#ifdef ESP_PLATFORM
    init_espnow();
#endif
    set_mesh_send_handler(driver_send);
}

void set_radio_transport(RadioTransport mode) {
    g_transport_mode = mode;
}

RadioTransport current_radio_transport() {
    return g_transport_mode;
}

bool radio_driver_send(const EncryptedFrame& frame) {
    return driver_send(frame);
}
