#pragma once
#include <Arduino.h>
#include "payload.h"

// A rename command received in the downlink window (issue #21):
// {"rename": {"node_id": "...", "node_name": "..."}}. Filled by
// sendPayload; main.cpp applies it (saveConfig + reboot).
struct RenameCmd
{
    char node_id[16];   // new node_id, e.g. "indoor1"
    char node_name[32]; // new node_name, e.g. "Back porch"
    bool present;       // true if a valid rename arrived
};

// Sends the payload to the gateway by ESP-NOW unicast (MAC from NVS).
// Retries up to 3 times on no MAC-ACK, then gives up — the node sleeps
// regardless, never burns battery retrying. After sending, listens 2 s
// for a downlink command from the gateway.
// Returns true if an OTA command ({"ota": true, ...}) arrived in the
// window. If the command carries a "url", it is copied to urlOut
// (up to urlOutLen-1 chars); urlOut is empty when there is no URL.
// A rename command is reported via renameOut (present = true); the
// return value stays false (no OTA command).
bool sendPayload(const SensorPayload &pkt, const String &gwMacStr, char *urlOut, size_t urlOutLen, RenameCmd *renameOut);

// Sends a JSON status message (e.g. ota_ready) to the gateway by ESP-NOW.
// Same 3-attempt MAC-ACK retry as sendPayload. Returns true if delivered.
bool sendStatus(const String &gwMacStr, const String &json);
