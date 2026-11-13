#include "watchdog.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

#define WATCHDOG_TIMEOUT_MS 30000

void watchdogSetup()
{
    esp_task_wdt_config_t config = {
        .timeout_ms = WATCHDOG_TIMEOUT_MS,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // keep watching the IDLE tasks (the default)
        .trigger_panic = true,                           // a timeout panics + reboots, not just warns
    };
    // The Arduino core already initialized the watchdog (5 s default);
    // reconfigure it to our timeout. If init somehow never ran,
    // initialize it instead.
    if (esp_task_wdt_reconfigure(&config) != ESP_OK)
        esp_task_wdt_init(&config);
    esp_task_wdt_add(NULL); // watch the current (main) task
}

void watchdogFeed()
{
    esp_task_wdt_reset();
}
