#include "config.h"
#include <Preferences.h>
#include "portal.h"
#include "pairing.h"

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

void saveConfig(const NodeConfig &cfg)
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false); // false = read/write
    prefs.putString("wifi_ssid", cfg.wifi_ssid);
    prefs.putString("wifi_pass", cfg.wifi_pass);
    prefs.putString("gw_mac", cfg.gw_mac);
    prefs.putString("node_id", cfg.node_id);
    prefs.putString("node_name", cfg.node_name);
    prefs.end();
}

NodeConfig readStoredConfig()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, true); // true = read-only
    NodeConfig cfg;
    cfg.wifi_ssid = prefs.getString("wifi_ssid", "");
    cfg.gw_mac = prefs.getString("gw_mac", "");
    cfg.node_id = prefs.getString("node_id", "");
    cfg.node_name = prefs.getString("node_name", "");
    prefs.end();
    return cfg;
}

NodeConfig loadConfig()
{
    Preferences prefs;
    prefs.begin(NAMESPACE, false); // false = read/write

    PressType press = detectButtonPress();
    if (!allKeysPresent(prefs) || press == PressType::LONG)
    {
        prefs.end();
        runProvisioningPortal(); // never returns — reboots when done
    }

    NodeConfig cfg;
    cfg.wifi_ssid = prefs.getString("wifi_ssid", "");
    cfg.wifi_pass = prefs.getString("wifi_pass", "");
    cfg.gw_mac = prefs.getString("gw_mac", "");
    cfg.node_id = prefs.getString("node_id", "");
    cfg.node_name = prefs.getString("node_name", "");
    prefs.end();

    // Un-named node (issue #21): no node_id in NVS yet. Generate a
    // provisional ID from the chip MAC — deterministic, so the Pi's
    // record stays valid across reboots. Held in memory only, never
    // saved to NVS: the node is genuinely un-named until renamed.
    // (Dormant until module 5: allKeysPresent still routes a node with
    // no node_id to the portal, so this only runs once that relaxes.)
    if (cfg.node_id.isEmpty())
    {
        uint64_t mac = ESP.getEfuseMac();
        char prov[16];
        snprintf(prov, sizeof(prov), "node-%04X", (unsigned)(mac & 0xFFFF));
        cfg.node_id = prov;
        Serial.printf("No node_id in NVS — provisional ID: %s\n", prov);
    }

    if (press == PressType::TAP)
    {
        runPairingMode(cfg); // updates cfg.gw_mac on success; timeout keeps the old MAC
    }

    Serial.printf("Config: node %s (%s), gateway %s, SSID %s\n",
                  cfg.node_id.c_str(), cfg.node_name.c_str(),
                  cfg.gw_mac.c_str(), cfg.wifi_ssid.c_str());
    return cfg;
}
