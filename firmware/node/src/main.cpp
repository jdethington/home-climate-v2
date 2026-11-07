#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include "payload.h"
#include "config.h"    // loadConfig() - NVS, provisioning if empty
#include "sensing.h"   // readSensors() - DHT auto-detect
#include "transport.h" // sendPayload() - ESP-NOW unicast + downlink listen
#include "power.h"     // readBattery(), goToSleep()

#define SLEEP_SECONDS 300 // 5 minutes
#define DHT_PIN 4         // same pon as v1 hub DHT - sensing code
#define BATT_ADC_PIN 32   // ADC1 (ADC2 can't read while WiFi is active) - sensing code

RTC_DATA_ATTR uint32_t bootCount = 0; // incremented on every boot, survives deep sleep

void setup()
{
    Serial.begin(115200);
    delay(1000); // wait for serial to be ready
    bootCount++;

    // 1. Config - reads NVS; empty NVS drops into serial provisioning, then reboots
    NodeConfig cfg = loadConfig();

    // 2. Sense — retries internally; returns NaN temp/humidity if the sensor is dead,
    //    so the Pi sees "node alive, sensor dead" instead of silence
    SensorReading reading = readSensor(DHT_PIN);

    // 3. Battery — USB-powered indoor nodes just report the USB rail; Pi ignores it
    float battV = readBatteryV(BATT_ADC_PIN);

    // 4. Fill the payload
    SensorPayload pkt = {};
    snprintf(pkt.node_id, sizeof(pkt.node_id), "%s", cfg.node_id.c_str());
    snprintf(pkt.location, sizeof(pkt.location), "%s", cfg.node_name.c_str());
    snprintf(pkt.sensor_model, sizeof(pkt.sensor_model), "%s", reading.model);
    snprintf(pkt.fw_version, sizeof(pkt.fw_version), "%s", FW_VERSION);
    pkt.temp_c = reading.temp_c;
    pkt.humidity = reading.humidity;
    pkt.batt_v = battV;
    pkt.boot_count = bootCount;

    Serial.printf("Node %s: %.1fC %.1f%% batt %.2fV boot %lu\n",
                  pkt.node_id, pkt.temp_c, pkt.humidity, pkt.batt_v, pkt.boot_count);

    // 5. Send — unicast to gw_mac, 2–3 retries on no MAC-ACK, then sleep regardless;
    //    stays awake 2 s after for a downlink command. Wi-Fi/ESP-NOW init lives in here.
    sendPayload(pkt, cfg.gw_mac);

    // 6. Deep sleep — the chip restarts from the top of setup() on wake
    goToSleep(SLEEP_SECONDS);
}

void loop()
{
    // empty — deep sleep restarts the chip, so setup() runs the whole cycle
}