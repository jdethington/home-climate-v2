#pragma once
#include <Arduino.h>

struct NodeConfig
{
    String wifi_ssid; // Wi-Fi SSID for provisioning
    String wifi_pass; // Wi-Fi password for provisioning
    String gw_mac;    // AA:BB:CC:DD:EE:FF — MAC of the gateway to unicast to
    String node_id;   // e.g. "indoor1" — matches MQTT topic, SensorPayload.node_id
    String node_name; // e.g. "Back porch" — matches SensorPayload.location
};

// Reads NVS ("climate" namespace). If nothing is stored yet, starts the
// captive portal (ClimateNode-Setup AP) — the setup page collects the
// config, saves it, and reboots. Never returns unconfigured.
NodeConfig loadConfig();

// Writes all five keys to NVS ("climate" namespace).
void saveConfig(const NodeConfig &cfg);

// Reads whatever is stored (possibly partial) without triggering
// provisioning. Used to pre-fill the setup form; the Wi-Fi password is
// never read back here — that field always starts empty.
NodeConfig readStoredConfig();
