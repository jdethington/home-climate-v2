#include "espnow.h"
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include "payload.h"
#include "mqtt.h" // mqttPublish()

#define RX_LED_PIN 2
#define MAX_NODES 8

struct NodeEntry
{
    char node_id[16];
    uint8_t mac[6];
    bool used;
};

static NodeEntry nodes[MAX_NODES];
static volatile unsigned long rxLedOnMs = 0;

static NodeEntry *findNode(const char *nodeId)
{
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (nodes[i].used && strcmp(nodes[i].node_id, nodeId) == 0)
            return &nodes[i];
    }
    return nullptr;
}

// Remember which MAC belongs to which node_id, from the sender address
// of each received packet. The payload carries no MAC, so this table is
// how downlink commands find their node.
static void learnMac(const char *nodeId, const uint8_t *mac)
{
    NodeEntry *e = findNode(nodeId);
    if (e == nullptr)
    {
        for (int i = 0; i < MAX_NODES; i++)
        {
            if (!nodes[i].used)
            {
                e = &nodes[i];
                break;
            }
        }
    }
    if (e == nullptr)
        return; // table full — node stays unknown
    snprintf(e->node_id, sizeof(e->node_id), "%s", nodeId);
    memcpy(e->mac, mac, 6);
    e->used = true;
}

static void onEspNowReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != (int)sizeof(SensorPayload))
    {
        Serial.printf("ESP-NOW: wrong size %d, ignored\n", len);
        return;
    }
    SensorPayload pkt;
    memcpy(&pkt, data, sizeof(pkt));

    digitalWrite(RX_LED_PIN, HIGH);
    rxLedOnMs = millis();

    learnMac(pkt.node_id, info->src_addr);

    // Payload -> JSON. NaN temp/humidity serialize as null.
    JsonDocument doc;
    doc["node_id"] = pkt.node_id;
    doc["location"] = pkt.location;
    doc["sensor_model"] = pkt.sensor_model;
    doc["fw_version"] = pkt.fw_version;
    doc["temp_c"] = pkt.temp_c;
    doc["humidity"] = pkt.humidity;
    doc["batt_v"] = pkt.batt_v;
    doc["boot_count"] = pkt.boot_count;
    String json;
    serializeJson(doc, json);

    String topic = "homeclimate/sensor/" + String(pkt.node_id);
    if (mqttPublish(topic, json))
    {
        Serial.printf("RX %s: %.1fC %.1f%% -> %s\n", pkt.node_id, pkt.temp_c, pkt.humidity, topic.c_str());
    }
    else
    {
        Serial.println("RX: MQTT not connected, reading dropped");
    }
}

bool espnowSend(const String &nodeId, const uint8_t *data, unsigned int length)
{
    NodeEntry *e = findNode(nodeId.c_str());
    if (e == nullptr)
    {
        Serial.printf("CMD: unknown node %s (no packet received from it yet)\n", nodeId.c_str());
        return false;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, e->mac, 6);
    peer.channel = WiFi.channel();
    peer.encrypt = false;
    // add_peer fails if the peer already exists — that's fine, ignore it
    esp_now_add_peer(&peer);
    esp_err_t res = esp_now_send(e->mac, data, (int)length);
    if (res != ESP_OK)
    {
        Serial.printf("CMD: esp_now_send failed (%d)\n", (int)res);
        return false;
    }
    return true;
}

void espnowSetup()
{
    pinMode(RX_LED_PIN, OUTPUT);
    digitalWrite(RX_LED_PIN, LOW);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onEspNowReceive);
    Serial.println("ESP-NOW ready");
}

void espnowLoop()
{
    if (rxLedOnMs != 0 && millis() - rxLedOnMs > 150)
    {
        digitalWrite(RX_LED_PIN, LOW);
        rxLedOnMs = 0;
    }
}
