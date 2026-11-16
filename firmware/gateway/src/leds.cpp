#include "leds.h"
#include <WiFi.h>

#define LED_WIFI 16
#define LED_RGB_R 13
#define LED_RGB_G 14
#define LED_RGB_B 18
#define LED_INDOOR 19
#define LED_OUTDOOR 25 // v1 moved it off 21: keep 21/22 free for I2C

#define LED_FREQ 5000
#define LED_RES 8

// Freshness tiers — nodes report every 5 min, so each missed cycle
// drops a brightness level.
#define FRESH_T1_MS 300000UL // < 5 min: full
#define FRESH_T2_MS 600000UL // < 10 min: 2/3
#define FRESH_T3_MS 900000UL // < 15 min: 1/3
                             // > 15 min: off

static unsigned long lastIndoorMs = 0;
static unsigned long lastOutdoorMs = 0;

static void setRgb(uint8_t r, uint8_t g, uint8_t b)
{
    ledcWrite(LED_RGB_R, r);
    ledcWrite(LED_RGB_G, g);
    ledcWrite(LED_RGB_B, b);
}

static uint8_t freshnessDuty(unsigned long lastMs, unsigned long now)
{
    if (lastMs == 0)
        return 0;
    unsigned long age = now - lastMs;
    if (age < FRESH_T1_MS)
        return 255;
    if (age < FRESH_T2_MS)
        return 170;
    if (age < FRESH_T3_MS)
        return 85;
    return 0;
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
}

void ledsLoop()
{
    digitalWrite(LED_WIFI, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
    unsigned long now = millis();
    ledcWrite(LED_INDOOR, freshnessDuty(lastIndoorMs, now));
    ledcWrite(LED_OUTDOOR, freshnessDuty(lastOutdoorMs, now));
}

void ledsMarkZoneFresh(bool indoor)
{
    if (indoor)
        lastIndoorMs = millis();
    else
        lastOutdoorMs = millis();
}

void ledsSetVerdict(bool open)
{
    if (open)
        setRgb(0, 180, 0); // green
    else
        setRgb(200, 0, 0); // red
}
