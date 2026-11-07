#include "config.h"
#include <Preferences.h>

static const char *NAMESPACE = "climate";
static const char *KEYS[] = {"wifi_ssid", "wifi_pass", "gw_mac", "node_id", "node_name"};
static const int NUM_KEYS = 5;

// True once every key has a non-empty value in NVS.
static bool allKeysPresent(Preferences &prefs)
{
    for (int i = 0; i < NUM_KEYS; i++)
    {
        if (prefs.getString(KEYS[i], "").length() == 0)
            return false;
    }
    return true;
}

// One-time setup over USB serial. Blocks until all five keys are stored,
// then reboots into normal operation.
static void provision(Preferences &prefs)
{
    Serial.println("NVS empty — provisioning mode.");
    Serial.println("Send one key=value line per key, then 'done':");
    Serial.println("  wifi_ssid, wifi_pass, gw_mac, node_id, node_name");
    Serial.println("Example: wifi_ssid=Brimhall22");

    while (true)
    {
        while (!Serial.available())
            delay(10);
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;

        if (line == "done")
        {
            if (allKeysPresent(prefs))
            {
                Serial.println("All keys stored - rebooting...");
                Serial.flush();
                ESP.restart();
            }
            Serial.println("Still missing keys - keep sending key=value lines, then 'done'.");
            continue;
        }

        int eq = line.indexOf('=');
        if (eq <= 0)
        {
            Serial.println("Bad line - use key=value");
            continue;
        }
        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);
        key.trim();
        value.trim();

        bool known = false;
        for (int i = 0; i < NUM_KEYS; i++)
        {
            if (key == KEYS[i])
            {
                prefs.putString(KEYS[i], value);
                if (key == "wifi_pass")
                {
                    Serial.println("wifi_pass stored (not shown)");
                }
                else
                {
                    Serial.printf("%s = %s\n", key.c_str(), value.c_str());
                }
                known = true;
                break;
            }
        }
        if (!known)
            Serial.println("Unknown key — ignored.");
    }
}

NodeConfig loadConfig()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false); // false = read/write

    if (!allKeysPresent(prefs))
    {
        provision(prefs); // never returns — reboots when done
    }

    NodeConfig cfg;
    cfg.wifi_ssid = prefs.getString("wifi_ssid", "");
    cfg.wifi_pass = prefs.getString("wifi_pass", "");
    cfg.gw_mac = prefs.getString("gw_mac", "");
    cfg.node_id = prefs.getString("node_id", "");
    cfg.node_name = prefs.getString("node_name", "");
    prefs.end();

    Serial.printf("Config: node %s (%s), gateway %s, SSID %s\n",
                  cfg.node_id.c_str(), cfg.node_name.c_str(),
                  cfg.gw_mac.c_str(), cfg.wifi_ssid.c_str());
    return cfg;
}