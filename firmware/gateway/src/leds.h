#pragma once
#include <Arduino.h>

// Indication LEDs (same breadboard + wiring as the v1 hub).
// RX (GPIO 2) lives in espnow.cpp — already there, untouched.
//
// Zone LEDs are bright when every known node in the zone is reporting,
// dim + blinking the missing node numbers (once per minute) when one
// is stale (>6 min — one 5-min cycle + 1 min of slack).
void ledsSetup();
void ledsLoop();
// Call when a sensor reading arrives — marks that node fresh.
// sensorOk is false when the node's sensor is dead (NaN readings).
void ledsMarkNodeFresh(const char *nodeId, bool sensorOk);
// Call when a recommendation verdict arrives — green OPEN, red CLOSED.
void ledsSetVerdict(bool open);
