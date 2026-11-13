#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "payload.h"
#include "gwconfig.h" // loadGatewayConfig() - Wi-Fi creds from NVS
#include "mqtt.h"     // mqttSetup(), mqttLoop()
#include "espnow.h"   // espnowSetup() - ESP-NOW init + receive callback
#include "ota.h"      // otaSetup(), otaLoop() - ElegantOTA update server

void setup()
{
    Serial.begin(115200);
    delay(1000);

    GatewayConfig cfg = loadGatewayConfig();

    // Wi-Fi first: the AP's channel becomes the ESP-NOW channel
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_pass.c_str());
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.printf("Wi-Fi connected: %s, channel %ld, IP %s\n",
                  WiFi.SSID().c_str(), WiFi.channel(),
                  WiFi.localIP().toString().c_str());

    espnowSetup(); // ESP-NOW init + receive callback (rides the Wi-Fi channel)
    mqttSetup();   // MQTT connect + subscribe homeclimate/cmd/#
    otaSetup();    // ElegantOTA update page (Wi-Fi is already up)
}

void loop()
{
    mqttLoop();   // keeps MQTT alive, reconnects if needed
    espnowLoop(); // turns off the RX LED after its blink
    otaLoop();    // serves the /update page
    delay(10);
}
