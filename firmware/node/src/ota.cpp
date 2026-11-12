#include "ota.h"
#include <WiFi.h>
#include <HTTPUpdate.h>
#include "transport.h" // sendStatus()

#define WIFI_CONNECT_TIMEOUT_MS 15000

void runOtaMode(const NodeConfig &cfg, const char *url)
{
    Serial.println("OTA mode: joining Wi-Fi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS)
        delay(250);
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("OTA mode: Wi-Fi connect failed, rebooting");
        delay(100);
        ESP.restart();
    }
    String ip = WiFi.localIP().toString();
    Serial.printf("OTA mode: Wi-Fi connected, IP %s\n", ip.c_str());

    // Announce readiness via ESP-NOW (still alive on channel 6 — the AP is
    // on the same channel, so joining Wi-Fi didn't move the radio). The
    // gateway clears the queued command on ota_ready.
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"node_id\":\"%s\",\"status\":\"ota_ready\",\"ip\":\"%s\"}",
             cfg.node_id.c_str(), ip.c_str());
    if (sendStatus(cfg.gw_mac, String(buf)))
        Serial.println("OTA mode: ota_ready sent");
    else
        Serial.println("OTA mode: ota_ready send failed (continuing anyway)");

    // Download the firmware from the URL and flash it. HTTPUpdate verifies
    // the image (including the server's X-MD5 header, when sent) and
    // reboots into the new firmware on success — this never returns then.
    // A failed download is safe: the old firmware keeps running.
    Serial.printf("OTA mode: downloading firmware from %s\n", url);
    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, url);
    if (ret == HTTP_UPDATE_OK)
    {
        // unreachable in practice — update() reboots on success
        Serial.println("OTA mode: update OK, rebooting");
    }
    else
    {
        Serial.printf("OTA mode: update failed (%d): %s\n", (int)ret, httpUpdate.getLastErrorString().c_str());
    }
    delay(100);
    ESP.restart();
}
