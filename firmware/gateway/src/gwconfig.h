#pragma once
#include <Arduino.h>

struct GatewayConfig
{
    String wifi_ssid;
    String wifi_pass;
};

// Reads Wi-Fi credentials from NVS (namespace "climate"). If either is
// missing, starts the captive portal (ClimateNode-Setup AP) — the setup
// page collects the credentials, saves them, and reboots. Never returns
// unconfigured.
GatewayConfig loadGatewayConfig();

// Writes both keys to NVS ("climate" namespace).
void saveGatewayConfig(const GatewayConfig &cfg);

// Reads whatever is stored (possibly partial) without triggering
// provisioning. Used to pre-fill the setup form; the Wi-Fi password is
// never read back here — that field always starts empty.
GatewayConfig readStoredGatewayConfig();
