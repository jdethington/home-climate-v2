#pragma once
#include <Arduino.h>

struct GatewayConfig
{
    String wifi_ssid;
    String wifi_pass;
};

// Reads Wi-Fi credentials from NVS (namespace "climate"). If either is
// missing, enters serial provisioning, then reboots.
GatewayConfig loadGatewayConfig();
