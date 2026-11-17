#include "leds.h"
#include <WiFi.h>
#include <cstring>
#include "parse.h" // validNodeId()

#define LED_WIFI 16
#define LED_RGB_R 13
#define LED_RGB_G 14
#define LED_RGB_B 18
#define LED_INDOOR 19
#define LED_OUTDOOR 25 // v1 moved it off 21: keep 21/22 free for I2C

#define LED_FREQ 5000
#define LED_RES 8

// Stale threshold — nodes report every 5 min, so 6 min gives 1 min slack.
#define STALE_MS 360000UL
// Blink cadence + timing.
#define BLINK_INTERVAL_MS 60000UL // blink the missing node numbers once per minute
#define BLINK_ON_MS 200           // each blink: 200 ms bright
#define BLINK_OFF_MS 200          // 200 ms dim between blinks
#define BLINK_GAP_MS 600          // 600 ms dim between node numbers

#define DUTY_FULL 255
#define DUTY_DIM 64 // quarter brightness — a node in the zone is stale

#define MAX_TRACKED_NODES 8 // matches the gateway's ESP-NOW peer table

struct TrackedNode
{
    char nodeId[16];
    unsigned long lastMs; // last report (millis)
};

static TrackedNode nodes[MAX_TRACKED_NODES];
static uint8_t nodeCount = 0;

enum class BlinkPhase
{
    IDLE,
    ON,
    OFF,
    GAP
};

struct ZoneBlink
{
    BlinkPhase phase;
    unsigned long phaseStartMs;
    unsigned long lastSeqMs; // start of the last blink sequence
    uint8_t missing[MAX_TRACKED_NODES];
    uint8_t missingCount;
    uint8_t nodeIdx;  // which missing node we're blinking
    uint8_t blinkIdx; // which blink within that node's number
};

static ZoneBlink indoorBlink;
static ZoneBlink outdoorBlink;

static void setRgb(uint8_t r, uint8_t g, uint8_t b)
{
    ledcWrite(LED_RGB_R, r);
    ledcWrite(LED_RGB_G, g);
    ledcWrite(LED_RGB_B, b);
}

// Zone of a node ID: true = indoor, false = outdoor. Only called for
// IDs that passed validNodeId(), so the prefix is always one of the two.
static bool isIndoor(const char *nodeId)
{
    return strncmp(nodeId, "indoor", 6) == 0;
}

// Trailing number of a node ID ("indoor2" -> 2, "outdoor12" -> 12).
static uint8_t nodeNumber(const char *nodeId)
{
    const char *p = nodeId;
    while (*p && (*p < '0' || *p > '9'))
        p++;
    uint8_t n = 0;
    while (*p >= '0' && *p <= '9')
        n = n * 10 + (*p++ - '0');
    return n;
}

void ledsMarkNodeFresh(const char *nodeId)
{
    // Only named indoor/outdoor nodes are tracked — provisional IDs
    // (node-XXXX) are excluded until the node is named.
    if (!validNodeId(nodeId))
        return;
    for (uint8_t i = 0; i < nodeCount; i++)
    {
        if (strcmp(nodes[i].nodeId, nodeId) == 0)
        {
            nodes[i].lastMs = millis();
            return;
        }
    }
    if (nodeCount >= MAX_TRACKED_NODES)
    {
        Serial.printf("LEDS: node table full, ignoring %s\n", nodeId);
        return;
    }
    snprintf(nodes[nodeCount].nodeId, sizeof(nodes[nodeCount].nodeId), "%s", nodeId);
    nodes[nodeCount].lastMs = millis();
    nodeCount++;
    Serial.printf("LEDS: tracking %s (%u nodes)\n", nodeId, nodeCount);
}

// Drive one zone LED: off (no known nodes), full (all fresh), or dim
// with a once-per-minute blink of the missing node numbers.
static void updateZoneLed(int pin, bool indoor, ZoneBlink &b, unsigned long now)
{
    uint8_t known = 0;
    uint8_t stale = 0;
    for (uint8_t i = 0; i < nodeCount; i++)
    {
        if (isIndoor(nodes[i].nodeId) != indoor)
            continue;
        known++;
        if (now - nodes[i].lastMs > STALE_MS)
            stale++;
    }
    if (known == 0)
    {
        ledcWrite(pin, 0);
        b.phase = BlinkPhase::IDLE;
        return;
    }
    if (stale == 0)
    {
        ledcWrite(pin, DUTY_FULL);
        b.phase = BlinkPhase::IDLE;
        return;
    }

    // At least one node in the zone is stale: dim base, blink the missing
    // node numbers once per minute.
    switch (b.phase)
    {
    case BlinkPhase::IDLE:
        ledcWrite(pin, DUTY_DIM);
        if (now - b.lastSeqMs >= BLINK_INTERVAL_MS)
        {
            // Build the missing-node list. Node 0 (indoor0, the gateway's
            // own sensor) is skipped — its failure is a sensor failure,
            // which gets its own pattern (module 2).
            b.missingCount = 0;
            for (uint8_t i = 0; i < nodeCount; i++)
            {
                if (isIndoor(nodes[i].nodeId) != indoor)
                    continue;
                if (now - nodes[i].lastMs <= STALE_MS)
                    continue;
                uint8_t num = nodeNumber(nodes[i].nodeId);
                if (num == 0)
                    continue;
                b.missing[b.missingCount++] = num;
            }
            if (b.missingCount == 0)
                return; // only indoor0 is stale — dim, no blink (module 2)
            b.nodeIdx = 0;
            b.blinkIdx = 0;
            b.phase = BlinkPhase::ON;
            b.phaseStartMs = now;
            b.lastSeqMs = now;
            ledcWrite(pin, DUTY_FULL);
        }
        break;
    case BlinkPhase::ON:
        if (now - b.phaseStartMs >= BLINK_ON_MS)
        {
            b.phase = BlinkPhase::OFF;
            b.phaseStartMs = now;
            ledcWrite(pin, DUTY_DIM);
        }
        break;
    case BlinkPhase::OFF:
        if (now - b.phaseStartMs >= BLINK_OFF_MS)
        {
            b.blinkIdx++;
            if (b.blinkIdx < b.missing[b.nodeIdx])
            {
                b.phase = BlinkPhase::ON;
                b.phaseStartMs = now;
                ledcWrite(pin, DUTY_FULL);
            }
            else
            {
                b.nodeIdx++;
                b.blinkIdx = 0;
                if (b.nodeIdx < b.missingCount)
                {
                    b.phase = BlinkPhase::GAP;
                    b.phaseStartMs = now;
                    // LED stays dim through the gap.
                }
                else
                {
                    b.phase = BlinkPhase::IDLE;
                    ledcWrite(pin, DUTY_DIM);
                }
            }
        }
        break;
    case BlinkPhase::GAP:
        if (now - b.phaseStartMs >= BLINK_GAP_MS)
        {
            b.phase = BlinkPhase::ON;
            b.phaseStartMs = now;
            ledcWrite(pin, DUTY_FULL);
        }
        break;
    }
}

void ledsSetup()
{
    pinMode(LED_WIFI, OUTPUT);
    ledcAttach(LED_RGB_R, LED_FREQ, LED_RES);
    ledcAttach(LED_RGB_G, LED_FREQ, LED_RES);
    ledcAttach(LED_RGB_B, LED_FREQ, LED_RES);
    ledcAttach(LED_INDOOR, LED_FREQ, LED_RES);
    ledcAttach(LED_OUTDOOR, LED_FREQ, LED_RES);
    digitalWrite(LED_WIFI, LOW);
    ledcWrite(LED_INDOOR, 0);
    ledcWrite(LED_OUTDOOR, 0);
    setRgb(0, 0, 0); // off until the first verdict arrives
    indoorBlink.phase = BlinkPhase::IDLE;
    indoorBlink.lastSeqMs = 0;
    outdoorBlink.phase = BlinkPhase::IDLE;
    outdoorBlink.lastSeqMs = 0;
}

void ledsLoop()
{
    digitalWrite(LED_WIFI, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
    unsigned long now = millis();
    updateZoneLed(LED_INDOOR, true, indoorBlink, now);
    updateZoneLed(LED_OUTDOOR, false, outdoorBlink, now);
}

void ledsSetVerdict(bool open)
{
    if (open)
        setRgb(0, 180, 0); // green
    else
        setRgb(200, 0, 0); // red
}
