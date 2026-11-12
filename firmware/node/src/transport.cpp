#include "transport.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define ESPNOW_CHANNEL 6 // must match the gateway's Wi-Fi channel (the AP's channel)

// The send callback runs on the Wi-Fi task, not the main task — volatile
// so the wait loop actually sees the change.
static volatile bool sendDone = false;
static volatile bool sendOk = false;
static volatile bool downlinkGot = false;

static void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    (void)info; // we only care about the status, not the destination
    sendOk = (status == ESP_NOW_SEND_SUCCESS);
    sendDone = true;
}

static char downlinkBuf[128];

static void onDownlink(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    (void)info;
    int n = len < (int)sizeof(downlinkBuf) - 1 ? len : (int)sizeof(downlinkBuf) - 1;
    memcpy(downlinkBuf, data, n);
    downlinkBuf[n] = '\0';
    downlinkGot = true;
    Serial.printf("Downlink received: %d bytes\n", len);
}

static bool parseMac(const String &s, uint8_t mac[6])
{
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
        return false;
    for (int i = 0; i < 6; i++)
    {
        if (v[i] < 0 || v[i] > 255)
            return false;
        mac[i] = (uint8_t)v[i];
    }
    return true;
}

// One send attempt: queue the packet, then wait up to 500 ms for the
// MAC-ACK callback. esp_now_send() only queues — the callback is the truth.
static bool sendOnce(const uint8_t *peer, const SensorPayload &pkt)
{
    sendDone = false;
    sendOk = false;
    if (esp_now_send(peer, (uint8_t *)&pkt, sizeof(pkt)) != ESP_OK)
        return false;
    unsigned long start = millis();
    while (!sendDone && millis() - start < 500)
        delay(5);
    return sendDone && sendOk;
}

bool sendPayload(const SensorPayload &pkt, const String &gwMacStr)
{
    uint8_t gwMac[6];
    if (!parseMac(gwMacStr, gwMac))
    {
        Serial.printf("Bad gateway MAC in NVS - cannot send");
        return false;
    }

    WiFi.mode(WIFI_STA); // no WiFi.begin — nodes never join the AP
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK)
    {
        Serial.printf("ESP-NOW init failed");
        return false;
    }
    esp_now_register_send_cb(onSent);
    esp_now_register_recv_cb(onDownlink);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, gwMac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        Serial.printf("Failed to add gateway peer");
        return false;
    }

    bool delivered = false;
    for (int attempt = 1; attempt <= 3 && !delivered; attempt++)
    {
        delivered = sendOnce(gwMac, pkt);
        if (!delivered)
        {

            Serial.printf("Send attempt %d failed - retrying\n", attempt);
            delay(100); // small delay before retrying
        }
    }
    Serial.println(delivered ? "Delivered (MAC-ACK)" : "Failed to deliver after 3 attempts - sleeping anyway");

    // Downlink listen window: the gateway may send a command (OTA trigger).
    // The receive callback is live from esp_now_init, so a command arriving
    // even before this window still gets caught — the window is just a wait.
    unsigned long start = millis();
    while (millis() - start < 2000 && !downlinkGot)
        delay(10);
    if (!downlinkGot)
        return false;
    // The only command so far: {"ota": true}. Substring check is enough for
    // one fixed-shape command; if commands grow, switch to ArduinoJson.
    if (strstr(downlinkBuf, "\"ota\"") != nullptr && strstr(downlinkBuf, "true") != nullptr)
    {
        Serial.println("Downlink: OTA command received");
        return true;
    }
    Serial.printf("Downlink: unknown command ignored: %s\n", downlinkBuf);
    return false;
}

bool sendStatus(const String &gwMacStr, const String &json)
{
    uint8_t gwMac[6];
    if (!parseMac(gwMacStr, gwMac))
    {
        Serial.println("Bad gateway MAC in NVS - cannot send status");
        return false;
    }
    // The peer was added by sendPayload; add again harmlessly (fails if it
    // already exists, which we ignore) so this also works standalone.
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, gwMac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        sendDone = false;
        sendOk = false;
        if (esp_now_send(gwMac, (const uint8_t *)json.c_str(), json.length()) == ESP_OK)
        {
            unsigned long start = millis();
            while (!sendDone && millis() - start < 500)
                delay(5);
            if (sendDone && sendOk)
                return true;
        }
        Serial.printf("Status send attempt %d failed - retrying\n", attempt);
        delay(100);
    }
    return false;
}
