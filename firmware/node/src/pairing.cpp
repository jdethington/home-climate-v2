#include "pairing.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "payload.h"   // ESPNOW_CHANNEL
#include "transport.h" // sendStatus()
#include "watchdog.h"

#define PAIR_TIMEOUT_MS 60000 // listen for the beacon up to 60 s

static volatile bool beaconGot = false;
static char beaconMac[18] = {0}; // "AA:BB:CC:DD:EE:FF" + NUL

// Beacon is JSON: {"type":"beacon","gw_mac":"...","channel":6}.
// The sender address IS the gateway's MAC — no parsing needed for it.
static void onBeacon(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len <= 0 || len >= 128)
        return;
    char buf[128];
    memcpy(buf, data, len);
    buf[len] = '\0';
    if (strstr(buf, "\"type\":\"beacon\"") == nullptr)
        return; // not a beacon (e.g. another node's pairing announcement)
    snprintf(beaconMac, sizeof(beaconMac), "%02X:%02X:%02X:%02X:%02X:%02X",
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5]);
    beaconGot = true;
}

bool runPairingMode(NodeConfig &cfg)
{
    Serial.println("Pairing mode — listening for the gateway beacon (60 s)...");
    WiFi.mode(WIFI_STA); // no WiFi.begin — ESP-NOW only
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Pairing: ESP-NOW init failed");
        return false;
    }
    esp_now_register_recv_cb(onBeacon);
    beaconGot = false;
    unsigned long start = millis();
    while (millis() - start < PAIR_TIMEOUT_MS && !beaconGot)
    {
        watchdogFeed(); // the 60 s listen exceeds the 30 s timeout
        delay(50);
    }

    if (!beaconGot)
    {
        Serial.println("Pairing: no beacon heard — keeping the stored config");
        esp_now_deinit();
        return false;
    }
    cfg.gw_mac = String(beaconMac);
    saveConfig(cfg);
    Serial.printf("Pairing: gateway %s stored\n", beaconMac);

    // Announce ourselves so the gateway learns us (and forwards to the Pi).
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"type\":\"pair\",\"node_id\":\"%s\",\"mac\":\"%s\"}",
             cfg.node_id.c_str(), WiFi.macAddress().c_str());
    if (sendStatus(cfg.gw_mac, String(buf)))
        Serial.println("Pairing: announcement sent");
    else
        Serial.println("Pairing: announcement send failed (gateway learns us from the next reading anyway)");
    esp_now_deinit(); // sendPayload re-inits fresh for the normal cycle
    return true;
}
