#pragma once
#include <Arduino.h>

// Connects to the broker and subscribes to homeclimate/cmd/#.
// Call mqttLoop() from loop() — it pumps the client and reconnects.
void mqttSetup();
void mqttLoop();

// Publishes a payload to a topic. Returns false if not connected.
bool mqttPublish(const String &topic, const String &payload);
