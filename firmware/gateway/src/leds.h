#pragma once
#include <Arduino.h>

// Indication LEDs (same breadboard + wiring as the v1 hub).
// RX (GPIO 2) lives in espnow.cpp — already there, untouched.
void ledsSetup();
void ledsLoop();
// Call when a sensor reading arrives — marks the zone fresh.
void ledsMarkZoneFresh(bool indoor);
// Call when a recommendation verdict arrives — green OPEN, red CLOSED.
void ledsSetVerdict(bool open);
