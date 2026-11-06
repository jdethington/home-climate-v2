/*
 * ESP-NOW walk test (issue #5)
 * receiver: logs each packet's seq + RSSI, prints a summary per burst
 * sender:   sends 100 packets (seq 0-99), then lights the onboard LED solid
 * Flash with: pio run -e receiver -t upload  /  pio run -e sender -t upload
 * Both boards stay on Wi-Fi channel 1 (no WiFi.begin) — do not connect
 * either board to home Wi-Fi during the test.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#define PACKET_COUNT 100
#define SEND_INTERVAL_MS 100
#define LED_BUILTIN 2 // ESP32 onboard LED is GPIO2
#define BUTTON_PIN 0  // BOOT button on the dev board

struct TestPacket
{
    uint16_t seq;
    uint8_t location; // optional: can be used to identify the location of the sender
};

#ifdef ROLE_RECEIVER
int received = 0;
int rssiMin = 0, rssiMax = 0, rssiSum = 0;
uint8_t burstLocation = 0;
unsigned long lastPktMs = 0;
int lastSeq = -1;

void printSummary()
{
    if (received == 0)
        return; // nothing to print
    Serial.printf("Location %u - Burst complete: %d/%d received, RSSI avg %d min %d max %d\n",
                  burstLocation, received, PACKET_COUNT, rssiSum / received, rssiMin, rssiMax);
    received = 0;
    rssiMin = 0;
    rssiMax = -100;
    rssiSum = 0;
}

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (len != sizeof(TestPacket))
        return; // ignore malformed packets
    // Parse the received packet
    TestPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));
    int rssi = info->rx_ctrl->rssi;
    // New burst starting: seq wrapped to a low number (seq 0 itself may have
    // been lost), or the previous burst was already closed by the timeout.
    if ((lastSeq >= 0 && pkt.seq < lastSeq) || received == 0)
    {
        printSummary(); // prints nothing if received == 0
        burstLocation = pkt.location;
    }
    if (pkt.seq == 0)
    {
        received = 0;
        rssiMin = 0;
        rssiMax = -100;
        rssiSum = 0;
        burstLocation = pkt.location;
    }
    received++;
    rssiSum += rssi;
    if (rssi < rssiMin)
        rssiMin = rssi;
    if (rssi > rssiMax)
        rssiMax = rssi;
    // Serial.printf(" #%u RSSI %d dBm\n", pkt.seq, rssi);
    lastSeq = pkt.seq;
    lastPktMs = millis();
}
#endif

#ifdef ROLE_SENDER
uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t locationId = 1; // plain RAM — the board never resets during the walk

void sendBurst()
{
    TestPacket pkt;
    pkt.location = locationId;
    Serial.printf("Sending burst from location %d\n", locationId);
    for (uint16_t i = 0; i < PACKET_COUNT; i++)
    {
        pkt.seq = i;
        esp_now_send(broadcastAddr, (uint8_t *)&pkt, sizeof(pkt));
        digitalWrite(LED_BUILTIN, HIGH); // blink on each send
        delay(10);
        digitalWrite(LED_BUILTIN, LOW);
        delay(SEND_INTERVAL_MS);
    }
    digitalWrite(LED_BUILTIN, HIGH); // solid = done, move to next spot
    Serial.println("Burst done - press BOOT button to send again at the next spot");
    locationId++;
}
#endif

void setup()
{
    Serial.begin(115200);
    delay(1000);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // BOOT button, GPIO 0, active LOW
    digitalWrite(LED_BUILTIN, LOW);

    WiFi.mode(WIFI_STA); // no WiFi.begin - both boards stay on channel 1
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW init failed");
        return;
    }
#ifdef ROLE_RECEIVER
    esp_now_register_recv_cb(onReceive);
    Serial.println("Receiver ready - waiting for bursts");
#endif
#ifdef ROLE_SENDER
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastAddr, 6);
    peer.channel = 1;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        Serial.println("Failed to add broadcast peer");
        return;
    }
    Serial.println("Sender ready - sending burst");
    sendBurst();
#endif
}

void loop()
{
#ifdef ROLE_SENDER
    // Check the state of the button
    if (digitalRead(BUTTON_PIN) == LOW)
    {
        delay(50); // debounce
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            sendBurst(); // uses locationId, then increments it
            while (digitalRead(BUTTON_PIN) == LOW)
                delay(10); // wait for release
        }
    }
#endif
#ifdef ROLE_RECEIVER
    // Burst over: silence for 2 s (covers a lost seq 99, and the last burst
    // of the walk, which has no next burst to trigger the split).
    if (received > 0 && millis() - lastPktMs > 2000)
    {
        printSummary();
        lastSeq = -1;
    }
#endif
    delay(10);
}