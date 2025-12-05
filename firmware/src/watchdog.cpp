#include "watchdog.hpp"

#if defined(OL_HW_WDT) && defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "esp_task_wdt.h"
#endif

void watchdog_init(uint32_t timeout_ms) {
#if defined(OL_HW_WDT) && defined(ESP_PLATFORM)
    const uint32_t timeout_s = timeout_ms == 0 ? 5 : (timeout_ms + 999) / 1000;
    esp_task_wdt_config_t cfg = {
        .timeout_ms = timeout_s * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = false,
    };
    esp_task_wdt_init(&cfg);
#else
    (void)timeout_ms;
#endif
}

void watchdog_register_task(const char* name, uint32_t timeout_ms) {
    (void)name;
#if defined(OL_HW_WDT) && defined(ESP_PLATFORM)
    (void)timeout_ms;
    esp_task_wdt_add(NULL); // add current task
#else
    (void)timeout_ms;
#endif
}

void watchdog_feed(const char* name) {
    (void)name;
#if defined(OL_HW_WDT) && defined(ESP_PLATFORM)
    esp_task_wdt_reset();
#endif
}
