#include "gwconfig.h"
#include <Preferences.h>

#define NAMESPACE "climate"

// The gateway only needs Wi-Fi credentials — no node identity, no
// gateway MAC. Same namespace as the node, but each board has its own
// NVS, so there's no conflict.
static const char *KEYS[] = {"wifi_ssid", "wifi_pass"};
static const int NUM_KEYS = 2;

static bool allKeysPresent(Preferences &prefs)
{
    for (int i = 0; i < NUM_KEYS; i++)
    {
        if (prefs.getString(KEYS[i], "").length() == 0)
            return false;
    }
    return true;
}

// First-boot provisioning over serial. Same pattern as the node:
// send "key=value" lines, then "done". Never echoes wifi_pass.
static void provision()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false);

    Serial.println("Gateway not configured. Send key=value lines, then 'done':");
    Serial.println("  wifi_ssid=MyWiFi");
    Serial.println("  wifi_pass=<your Wi-Fi password>");

    while (true)
    {
        if (Serial.available())
        {
            String line = Serial.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
                continue;

            if (line == "done")
            {
                if (allKeysPresent(prefs))
                {
                    Serial.println("Config complete — rebooting.");
                    prefs.end();
                    Serial.flush();
                    ESP.restart();
                }
                else
                {
                    Serial.println("Still missing keys — keep going, then send 'done'.");
                }
                continue;
            }

            int eq = line.indexOf('=');
            if (eq <= 0)
            {
                Serial.println("Bad line — use key=value");
                continue;
            }
            String key = line.substring(0, eq);
            String value = line.substring(eq + 1);

            bool known = false;
            for (int i = 0; i < NUM_KEYS; i++)
            {
                if (key == KEYS[i])
                {
                    known = true;
                    break;
                }
            }
            if (!known)
            {
                Serial.println("Unknown key: " + key);
                continue;
            }
            prefs.putString(key.c_str(), value);
            if (key == "wifi_pass")
            {
                Serial.println("wifi_pass stored (not shown)");
            }
            else
            {
                Serial.println(key + " = " + value);
            }
        }
        delay(10);
    }
}

GatewayConfig loadGatewayConfig()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    if (!allKeysPresent(prefs))
    {
        prefs.end();
        provision(); // never returns — reboots when done
    }

    GatewayConfig cfg;
    cfg.wifi_ssid = prefs.getString("wifi_ssid", "");
    cfg.wifi_pass = prefs.getString("wifi_pass", "");
    prefs.end();

    Serial.println("Gateway config: SSID " + cfg.wifi_ssid);
    return cfg;
}
