#include "sensors.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>

#ifdef ESP_PLATFORM
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#endif

namespace {
HealthStatus g_health{};

#ifdef ESP_PLATFORM
GpsStatus g_last_gps{};
constexpr uart_port_t kGnssUart = UART_NUM_1;
constexpr int kGnssTxPin = 17;
constexpr int kGnssRxPin = 16;
constexpr int kGnssBaud = 9600;
constexpr std::size_t kGnssBufLen = 256;
bool g_gnss_ready = false;

float parse_latlon(const char* field, const char hemi) {
    // NMEA lat/lon: DDMM.MMMM or DDDMM.MMMM
    if (field == nullptr || *field == '\0') {
        return 0.0f;
    }
    float raw = std::strtof(field, nullptr);
    const int degrees = static_cast<int>(raw / 100.0f);
    const float minutes = raw - (degrees * 100.0f);
    float deg = static_cast<float>(degrees) + (minutes / 60.0f);
    if (hemi == 'S' || hemi == 'W') {
        deg = -deg;
    }
    return deg;
}

bool parse_gga(const char* sentence, GpsStatus& out) {
    // Very small GGA parser for fix/lat/lon/sats/hdop/altitude.
    // Expected fields: $GPGGA,time,lat,N,lon,E,fix,sats,hdop,alt...
    std::array<const char*, 15> fields{};
    std::size_t idx = 0;
    const char* start = sentence;
    for (const char* p = sentence; *p && idx < fields.size(); ++p) {
        if (*p == ',') {
            fields[idx++] = start;
            start = p + 1;
        }
    }
    if (idx < 6) {
        return false;
    }

    const char* lat_field = fields[2];
    const char hemi_lat = fields[3] ? fields[3][0] : 'N';
    const char* lon_field = fields[4];
    const char hemi_lon = fields[5] ? fields[5][0] : 'E';
    const char* fix_field = fields[6];
    const char* sats_field = fields[7];
    const char* hdop_field = fields[8];
    const char* alt_field = fields[9];

    const int fix_quality = fix_field ? std::atoi(fix_field) : 0;
    out.valid_fix = fix_quality > 0;
    out.latitude_deg = parse_latlon(lat_field, hemi_lat);
    out.longitude_deg = parse_latlon(lon_field, hemi_lon);
    out.num_sats = static_cast<uint8_t>(sats_field ? std::atoi(sats_field) : 0);
    out.hdop = hdop_field ? std::strtof(hdop_field, nullptr) : 0.0f;
    out.altitude_m = alt_field ? std::strtof(alt_field, nullptr) : 0.0f;
    return true;
}

void read_gnss_uart() {
    if (!g_gnss_ready) {
        return;
    }
    std::array<uint8_t, kGnssBufLen> buf{};
    const int len = uart_read_bytes(kGnssUart, buf.data(), buf.size() - 1, 10 / portTICK_PERIOD_MS);
    if (len <= 0) {
        return;
    }
    buf[static_cast<std::size_t>(len)] = 0;

    // Parse line by line for GGA; keep last good fix.
    char* saveptr = nullptr;
    char* line = std::strtok_r(reinterpret_cast<char*>(buf.data()), "\r\n", &saveptr);
    while (line != nullptr) {
        if (std::strncmp(line, "$GPGGA", 6) == 0 || std::strncmp(line, "$GNGGA", 6) == 0) {
            GpsStatus parsed{};
            parsed.timestamp_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
            parsed.cn0_db_hz_avg = g_last_gps.cn0_db_hz_avg; // placeholder; unavailable in GGA
            parsed.jamming_detected = false;
            parsed.spoof_detected = false;
            if (parse_gga(line, parsed)) {
                g_last_gps = parsed;
            }
        }
        line = std::strtok_r(nullptr, "\r\n", &saveptr);
    }
}

void init_gnss_uart() {
    uart_config_t cfg{};
    cfg.baud_rate = kGnssBaud;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_APB;

    if (uart_param_config(kGnssUart, &cfg) != ESP_OK) {
        ESP_LOGE("GNSS", "uart_param_config failed");
        return;
    }
    if (uart_set_pin(kGnssUart, kGnssTxPin, kGnssRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE("GNSS", "uart_set_pin failed");
        return;
    }
    if (uart_driver_install(kGnssUart, kGnssBufLen, 0, 0, nullptr, 0) != ESP_OK) {
        ESP_LOGE("GNSS", "uart_driver_install failed");
        return;
    }
    g_gnss_ready = true;
    ESP_LOGI("GNSS", "GNSS UART ready (baud=%d tx=%d rx=%d)", kGnssBaud, kGnssTxPin, kGnssRxPin);
}
#endif
} // namespace

void init_sensors() {
#ifdef ESP_PLATFORM
    init_gnss_uart();
#endif
}

HealthStatus read_health_status(uint32_t now_ms) {
    g_health.timestamp_ms = now_ms;
    g_health.battery_v = 3.7f;
    g_health.temp_c = 25.0f;
    g_health.imu_tilt_deg = 0.5f;
    g_health.tamper_flag = false;
    return g_health;
}

GpsStatus read_gps_status(uint32_t now_ms) {
#ifdef ESP_PLATFORM
    read_gnss_uart();
    g_last_gps.timestamp_ms = now_ms;
    return g_last_gps;
#else
    GpsStatus gps{};
    gps.timestamp_ms = now_ms;
    gps.latitude_deg = 37.7749f;
    gps.longitude_deg = -122.4194f;
    gps.altitude_m = 10.0f;
    gps.num_sats = 7;
    gps.hdop = 1.2f;
    gps.valid_fix = true;
    gps.jamming_detected = false;
    gps.spoof_detected = false;
    gps.cn0_db_hz_avg = 38.0f;
    return gps;
#endif
}
