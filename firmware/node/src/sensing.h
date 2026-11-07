#pragma once
#include <Arduino.h>

struct SensorReading
{
    float temp_c;
    float humidity;
    char model[8]; // "DHT22" or "DHT11", "" if unknown
    bool ok;
};

// Reads the DHT sensor on the given pin. Auto-detects DHT22 vs DHT11 on
// first use (and re-detects after 3 failed wakes, for hot-swaps). Returns
// ok=false with NaN values if the sensor doesn't answer.
SensorReading readSensor(uint8_t pin);