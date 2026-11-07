#pragma once
#include <Arduino.h>
#include "payload.h"

// Sends the payload to the gateway by ESP-NOW unicast (MAC from NVS).
// Retries up to 3 times on no MAC-ACK, then gives up and returns — the
// node sleeps regardless, never burns battery retrying. After sending,
// listens 2 s for a downlink command from the gateway.
void sendPayload(const SensorPayload &pkt, const String &gwMacStr);
