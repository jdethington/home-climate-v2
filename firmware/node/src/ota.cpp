#include "ota.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "transport.h" // sendStatus()

#define OTA_TIMEOUT_MS (10UL * 60UL * 1000UL) // 10 minutes
#define WIFI_CONNECT_TIMEOUT_MS 15000

void runOtaMode(const NodeConfig &cfg)
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
    // gateway forwards this to homeclimate/status/<node-id> for the dashboard.
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"node_id\":\"%s\",\"status\":\"ota_ready\",\"ip\":\"%s\"}",
             cfg.node_id.c_str(), ip.c_str());
    if (sendStatus(cfg.gw_mac, String(buf)))
        Serial.println("OTA mode: ota_ready sent");
    else
        Serial.println("OTA mode: ota_ready send failed (continuing anyway)");

    WebServer server(80);
    // ElegantOTA serves its page at /update, not / — redirect the bare IP
    // so http://<node-ip>/ lands on the upload page.
    server.on("/", [&server]()
              {
    server.sendHeader("Location", "/update");
    server.send(302, "text/plain", ""); });
    ElegantOTA.begin(&server);
    server.begin();
    Serial.printf("OTA mode: ElegantOTA ready at http://%s (10 min timeout)\n", ip.c_str());

    // Serve uploads until the timeout. A reboot mid-upload is safe: OTA
    // writes to the inactive slot, and the bootloader only switches slots
    // after a complete, verified write — the old firmware keeps running.
    unsigned long otaStart = millis();
    while (millis() - otaStart < OTA_TIMEOUT_MS)
    {
        server.handleClient();
        ElegantOTA.loop();
        delay(10);
    }
    Serial.println("OTA mode: timeout, rebooting to normal cycle");
    delay(100);
    ESP.restart();
}
