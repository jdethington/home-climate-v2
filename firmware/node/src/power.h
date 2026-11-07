#pragma once
#include <Arduino.h>

// Reads the supply voltage via a 100k/100k divider on an ADC1 pin.
// Battery nodes: divider across the cell (3.0–4.2V). USB nodes: divider
// on VIN, so it reads the ~5V USB rail instead. Returns volts.
float readBatteryV(uint8_t pin);

// Deep sleep for the given seconds. Never returns — on wake the chip
// restarts at setup(). Flushes Serial first so debug output isn't lost.
void goToSleep(uint32_t seconds);
