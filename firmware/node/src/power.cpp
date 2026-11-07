#include "power.h"
#include <esp_sleep.h>

#define DIVIDER_RATIO 2.0 // 100k/100k divider: supply = 2x pin voltage

float readBatteryV(uint8_t pin)
{
    // 11 dB attenuation: the ADC reads ~0–3.9V. The default 0 dB only reads
    // 0–1.1V, and the divider gives up to 2.1V at the pin.
    analogSetPinAttenuation(pin, ADC_11db);
    // Average a few samples — the ESP32 ADC is noisy, and this is cheap.
    uint32_t sum = 0;
    const int SAMPLES = 8;
    for (int i = 0; i < SAMPLES; i++)
    {
        sum += analogReadMilliVolts(pin);
        delay(5);
    }
    float pinV = (sum / (float)SAMPLES) / 1000.0;
    return pinV * DIVIDER_RATIO;
}

void goToSleep(uint32_t seconds)
{
    Serial.printf("Sleeping %lu s...\n", seconds);
    Serial.flush(); // don't cut off the last lines mid-transmission
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start(); // never returns — wake restarts at setup()
}
