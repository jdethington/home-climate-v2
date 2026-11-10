#include "mqtt.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "espnow.h" // espnowSend() - forward cmd messages to nodes

#define BROKER_IP "192.168.0.104"
#define BROKER_PORT 1883
#define CLIENT_ID "climate-gateway"
#define CMD_TOPIC "homeclimate/cmd/#"

static WiFiClient wifiClient;
static PubSubClient client(wifiClient);
static unsigned long lastReconnectAttempt = 0;

static void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    // Topic: homeclimate/cmd/<node-id> — forward the raw payload to the node
    String t = String(topic);
    const String prefix = "homeclimate/cmd/";
    if (!t.startsWith(prefix))
        return;
    String nodeId = t.substring(prefix.length());
    Serial.printf("CMD for %s: %u bytes\n", nodeId.c_str(), length);
    espnowSend(nodeId, payload, length);
}

static bool reconnect()
{
    if (client.connect(CLIENT_ID))
    {
        Serial.println("MQTT connected");
        client.subscribe(CMD_TOPIC);
        Serial.println("Subscribed: homeclimate/cmd/#");
        return true;
    }
    Serial.printf("MQTT connect failed, rc=%d\n", client.state());
    return false;
}

void mqttSetup()
{
    client.setServer(BROKER_IP, BROKER_PORT);
    client.setCallback(onMqttMessage);
    client.setBufferSize(512);
    reconnect();
}

void mqttLoop()
{
    if (!client.connected())
    {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= 5000)
        {
            lastReconnectAttempt = now;
            reconnect();
        }
        return;
    }
    client.loop();
}

bool mqttPublish(const String &topic, const String &payload)
{
    if (!client.connected())
        return false;
    return client.publish(topic.c_str(), payload.c_str());
}
