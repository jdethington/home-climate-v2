#include "gwconfig.h"
#include <Preferences.h>
#include "portal.h"

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

void saveGatewayConfig(const GatewayConfig &cfg)
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false); // false = read/write
    prefs.putString("wifi_ssid", cfg.wifi_ssid);
    prefs.putString("wifi_pass", cfg.wifi_pass);
    prefs.end();
}

GatewayConfig readStoredGatewayConfig()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, true); // true = read-only
    GatewayConfig cfg;
    cfg.wifi_ssid = prefs.getString("wifi_ssid", "");
    prefs.end();
    return cfg;
}

GatewayConfig loadGatewayConfig()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    if (!allKeysPresent(prefs))
    {
        prefs.end();
        runProvisioningPortal(); // never returns — reboots when done
    }

    GatewayConfig cfg;
    cfg.wifi_ssid = prefs.getString("wifi_ssid", "");
    cfg.wifi_pass = prefs.getString("wifi_pass", "");
    prefs.end();

    Serial.println("Gateway config: SSID " + cfg.wifi_ssid);
    return cfg;
}
