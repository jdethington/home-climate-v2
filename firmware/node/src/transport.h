#pragma once
#include <Arduino.h>
#include "payload.h"

// Sends the payload to the gateway by ESP-NOW unicast (MAC from NVS).
// Retries up to 3 times on no MAC-ACK, then gives up — the node sleeps
// regardless, never burns battery retrying. After sending, listens 2 s
// for a downlink command from the gateway.
// Returns true if an OTA command ({"ota": true, ...}) arrived in the
// window. If the command carries a "url", it is copied to urlOut
// (up to urlOutLen-1 chars); urlOut is empty when there is no URL.
bool sendPayload(const SensorPayload &pkt, const String &gwMacStr, char *urlOut, size_t urlOutLen);

// Sends a JSON status message (e.g. ota_ready) to the gateway by ESP-NOW.
// Same 3-attempt MAC-ACK retry as sendPayload. Returns true if delivered.
bool sendStatus(const String &gwMacStr, const String &json);
