#pragma once
#include <Arduino.h>

struct NodeConfig {
    String wifi_ssid;   // Wi-Fi SSID for provisioning
    String wifi_pass;   // Wi-Fi password for provisioning
    String gw_mac;      // AA:BB:CC:DD:EE:FF — MAC of the gateway to unicast to
    String node_id;     // e.g. "indoor1" — matches MQTT topic, SensorPayload.node_id
    String node_name;   // e.g. "Back porch" — matches SensorPayload.location
};

// Reads NVS ("climate" namespace). If nothing is stored yet, drops into
// serial provisioning: send key=value lines, then "done". Reboots after.
NodeConfig loadConfig();