// firmware/shared/payload.h — wire contract between node, gateway, and Pi.

#pragma once

#include <cstdint>

#define FW_VERSION "1.1.7" // bump on every release
#define ESPNOW_CHANNEL 6   // must match the gateway's Wi-Fi channel (the AP's channel)

struct SensorPayload
{
    char node_id[16];     // e.g. "indoor1" — matches NVS node_id, MQTT topic
    char location[16];    // e.g. "Back porch" — matches NVS node_name
    char sensor_model[8]; // e.g. "DHT22"
    char fw_version[8];   // FW_VERSION, e.g. "1.0.0"
    float temp_c;
    float humidity;
    float batt_v;
    uint32_t boot_count;
};
