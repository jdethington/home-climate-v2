#include "sensing.h"
#include <DHT.h>
#include <Math.h>

// Must survive deep sleep (each wake is a fresh boot). RTC_DATA_ATTR
// re-initializes to 0 on power-up/reset — which is exactly the "unknown,
// detect again" state. Same memory, opposite lesson from the walk test:
// there the reset wiped the counter and broke us; here the wipe is what
// we want, because a reset means "forget what sensor was attached".
RTC_DATA_ATTR uint8_t detectedType = 0; // 0 = unknown, DHT11 or DHT22
RTC_DATA_ATTR uint8_t failCount = 0;

static bool saneReading(float t, float h)
{
    return !isnan(t) && !isnan(h) && t > -40.0f && t < 80.0f && h >= 0.0f && h <= 100.0f;
}

// One attempt with one sensor type. A fresh DHT object per attempt — the
// library fixes the type in the constructor, so switching types means a
// new object.
static bool tryRead(uint8_t pin, uint8_t type, float &t, float &h)
{
    DHT dht(pin, type);
    dht.begin();
    delay(100); // let the sensor settle after begin
    h = dht.readHumidity();
    t = dht.readTemperature();
    return saneReading(t, h);
}

SensorReading readSensor(uint8_t pin)
{
    SensorReading r = {NAN, NAN, "", false};

    // Fast path: try the type detected on a precious wake
    if (detectedType == DHT22 || detectedType == DHT11)
    {
        if (tryRead(pin, detectedType, r.temp_c, r.humidity))
        {
            failCount = 0;
            snprintf(r.model, sizeof(r.model),"DHT%d", detectedType);
            r.ok = true;
            return r;
        }
        failCount++;
        // One failed read is usually a glitch, not a swapped sensor — only
        // re-detect after 3 failed wakes in a row.
        if (failCount < 3)
            return r; // ok=false, NaN - try again next wake
    }

    // Full detection: DHT22 first, then DHT11
    if (tryRead(pin, DHT22, r.temp_c, r.humidity))
    {
        detectedType = DHT22;
        failCount = 0;
        snprintf(r.model, sizeof(r.model), "DHT22");
        r.ok = true;
        return r;
    }
    if (tryRead(pin, DHT11, r.temp_c, r.humidity))
    {
        detectedType = DHT11;
        failCount = 0;
        snprintf(r.model, sizeof(r.model), "DHT11");
        r.ok = true;
        return r;
    }

    // Nothing answered - stay "unknown" so the next wake re-detects
    detectedType = 0;
    failCount = 0;
    return r;
}