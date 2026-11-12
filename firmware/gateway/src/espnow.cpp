#include "espnow.h"
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include "payload.h"
#include "mqtt.h" // mqttPublish()

#define RX_LED_PIN 2
#define MAX_NODES 8
#define CMD_EXPIRE_MS (15UL * 60UL * 1000UL) // 15 min

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

struct PendingCmd
{
    char node_id[16];
    char payload[128];
    unsigned long created_ms;
    bool used;
};

static PendingCmd pending[MAX_NODES];

void queueCommand(const String &nodeId, const uint8_t *data, unsigned int length)
{
    // Try immediately in case the node happens to be awake right now.
    espnowSend(nodeId, data, length);
    // And queue it — the node reports every 5 min, so the next reading
    // delivers it for sure. Overwrites any older pending command.
    PendingCmd *slot = nullptr;
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (pending[i].used && strcmp(pending[i].node_id, nodeId.c_str()) == 0)
        {
            slot = &pending[i];
            break;
        }
    }
    if (slot == nullptr)
    {
        for (int i = 0; i < MAX_NODES; i++)
        {
            if (!pending[i].used)
            {
                slot = &pending[i];
                break;
            }
        }
    }
    if (slot == nullptr)
    {
        Serial.println("CMD queue full — command dropped");
        return;
    }
    snprintf(slot->node_id, sizeof(slot->node_id), "%s", nodeId.c_str());
    unsigned int n = length < sizeof(slot->payload) - 1 ? length : sizeof(slot->payload) - 1;
    memcpy(slot->payload, data, n);
    slot->payload[n] = '\0';
    slot->created_ms = millis();
    slot->used = true;
    Serial.printf("CMD queued for %s\n", nodeId.c_str());
}

// Forwards a node's pending command. Does NOT clear it — the entry stays
// until the node acks (ota_ready) or it expires, so a lost forward is
// retried on the next reading.
static void sendPending(const char *nodeId)
{
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (pending[i].used && strcmp(pending[i].node_id, nodeId) == 0)
        {
            Serial.printf("CMD: forwarding queued command to %s\n", nodeId);
            espnowSend(String(nodeId), (const uint8_t *)pending[i].payload, strlen(pending[i].payload));
            return;
        }
    }
}

static void clearPending(const char *nodeId)
{
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (pending[i].used && strcmp(pending[i].node_id, nodeId) == 0)
        {
            pending[i].used = false;
            Serial.printf("CMD: cleared pending command for %s\n", nodeId);
            return;
        }
    }
}

static void onEspNowReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    // JSON status message (starts with '{') — e.g. {"node_id":"indoor1",
    // "status":"ota_ready","ip":"..."}. Forward it to MQTT as-is.
    if (len > 0 && data[0] == '{')
    {
        char buf[256];
        if (len >= (int)sizeof(buf))
        {
            Serial.println("ESP-NOW: status too long, ignored");
            return;
        }
        memcpy(buf, data, len);
        buf[len] = '\0';
        JsonDocument doc;
        if (deserializeJson(doc, buf) != DeserializationError::Ok)
        {
            Serial.println("ESP-NOW: bad status JSON, ignored");
            return;
        }
        const char *nodeId = doc["node_id"];
        if (nodeId == nullptr)
        {
            Serial.println("ESP-NOW: status without node_id, ignored");
            return;
        }
        learnMac(nodeId, info->src_addr);
        digitalWrite(RX_LED_PIN, HIGH);
        rxLedOnMs = millis();
        String topic = "homeclimate/status/" + String(nodeId);
        if (mqttPublish(topic, String(buf)))
            Serial.printf("STATUS %s -> %s\n", nodeId, topic.c_str());
        else
            Serial.println("STATUS: MQTT not connected, status dropped");
        // The node acked — its queued command (if any) is done.
        if (doc["status"] == "ota_ready")
            clearPending(nodeId);
        return;
    }

    if (len != (int)sizeof(SensorPayload))
    {
        Serial.printf("ESP-NOW: wrong size %d, ignored\n", len);
        return;
    }

    SensorPayload pkt;
    memcpy(&pkt, data, sizeof(pkt));
    learnMac(pkt.node_id, info->src_addr);

    digitalWrite(RX_LED_PIN, HIGH);
    rxLedOnMs = millis();

    JsonDocument doc;
    doc["node_id"] = pkt.node_id;
    doc["location"] = pkt.location;
    doc["sensor_model"] = pkt.sensor_model;
    doc["fw_version"] = pkt.fw_version;
    doc["temp_c"] = pkt.temp_c;
    doc["humidity"] = pkt.humidity;
    doc["batt_v"] = pkt.batt_v;
    doc["boot_count"] = pkt.boot_count;

    String jsonStr;
    serializeJson(doc, jsonStr);

    String topic = "homeclimate/sensor/" + String(pkt.node_id);
    if (mqttPublish(topic, jsonStr))
        Serial.printf("RX %s: %.1fC %.1f%% -> %s\n", pkt.node_id, pkt.temp_c, pkt.humidity, topic.c_str());
    else
        Serial.println("RX: MQTT not connected, reading dropped");

    sendPending(pkt.node_id);
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
    // Expire queued commands older than 15 min (node never acked).
    unsigned long now = millis();
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (pending[i].used && now - pending[i].created_ms > CMD_EXPIRE_MS)
        {
            Serial.printf("CMD: expired queued command for %s\n", pending[i].node_id);
            pending[i].used = false;
        }
    }
}
