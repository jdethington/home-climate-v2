#pragma once
#include <Arduino.h>

// The gateway's own DHT11 (GPIO 4) — reports as indoor0 (backdoor).
void gwsensorSetup();
void gwsensorLoop(); // reads every 5 min, publishes to MQTT
