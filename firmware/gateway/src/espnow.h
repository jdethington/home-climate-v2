#pragma once
#include <Arduino.h>

// Initializes ESP-NOW and registers the receive callback. Call after
// Wi-Fi is connected — ESP-NOW rides the Wi-Fi channel.
void espnowSetup();

// Call from loop() — turns the RX LED off after its blink.
void espnowLoop();

// Sends a downlink command to a node by its node_id. The node's MAC is
// learned from received packets; returns false if the node is unknown.
bool espnowSend(const String &nodeId, const uint8_t *data, unsigned int length);

// Queues a downlink command for a node. The node is asleep most of the time,
// so the command is forwarded when its next reading arrives (it's awake and
// listening then). Cleared when the node acks (ota_ready) or after 15 min.
void queueCommand(const String &nodeId, const uint8_t *data, unsigned int length);
