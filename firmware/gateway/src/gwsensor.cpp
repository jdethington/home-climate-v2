#include "gwsensor.h"
#include <math.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "mqtt.h" // mqttPublish()
#include "leds.h" // ledsMarkNodeFresh()

#define DHT_PIN 4
#define DHT_TYPE DHT11
#define SENSOR_NODE_ID "indoor0"
#define SENSOR_LOCATION "backdoor"
#define SENSOR_MODEL "DHT11"
#define READ_INTERVAL_MS 300000UL // 5 min, matching the nodes

static DHT dht(DHT_PIN, DHT_TYPE);
static unsigned long lastReadMs = 0;

void gwsensorSetup()
{
    dht.begin();
}

void gwsensorLoop()
{
    unsigned long now = millis();
    if (lastReadMs != 0 && now - lastReadMs < READ_INTERVAL_MS)
        return;
    lastReadMs = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h))
    {
        Serial.println("GWSENSOR: DHT read failed");
        ledsMarkNodeFresh(SENSOR_NODE_ID, false);
        return;
    }

    JsonDocument doc;
    doc["node_id"] = SENSOR_NODE_ID;
    doc["location"] = SENSOR_LOCATION;
    doc["sensor_model"] = SENSOR_MODEL;
    doc["fw_version"] = "gateway";
    doc["temp_c"] = t;
    doc["humidity"] = h;
    doc["batt_v"] = nullptr; // mains-powered, no battery
    doc["boot_count"] = 0;

    String jsonStr;
    serializeJson(doc, jsonStr);

    String topic = "homeclimate/sensor/" + String(SENSOR_NODE_ID);
    if (mqttPublish(topic, jsonStr))
    {
        Serial.printf("GWSENSOR %s: %.1fC %.1f%% -> %s\n", SENSOR_NODE_ID, t, h, topic.c_str());
        ledsMarkNodeFresh(SENSOR_NODE_ID, true); // mark the gateway's own sensor fresh
    }
    else
        Serial.println("GWSENSOR: MQTT not connected, reading dropped");
}
