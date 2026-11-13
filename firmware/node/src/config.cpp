#include "config.h"
#include <Preferences.h>
#include "portal.h"
#include "pairing.h"

static const char *NAMESPACE = "climate";

// True once the Wi-Fi credentials are stored. Only Wi-Fi gates the
// portal now (issue #21): a missing gateway MAC is fixed by pairing,
// and a missing name is fixed from the dashboard — neither needs the
// portal.
static bool hasWifi(Preferences &prefs)
{
    return prefs.getString("wifi_ssid", "").length() > 0;
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
    if (!hasWifi(prefs) || press == PressType::LONG)
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
    if (cfg.node_id.isEmpty())
    {
        uint64_t mac = ESP.getEfuseMac();
        char prov[16];
        snprintf(prov, sizeof(prov), "node-%04X", (unsigned)(mac & 0xFFFF));
        cfg.node_id = prov;
        Serial.printf("No node_id in NVS — provisional ID: %s\n", prov);
    }

    // Pair when asked (tap), or automatically when the node has no
    // gateway MAC yet (issue #21 module 5): a node provisioned with
    // Wi-Fi only pairs itself on first boot instead of needing the
    // MAC typed in. A timeout keeps the old (empty) MAC and the node
    // sleeps, retrying next cycle.
    if (press == PressType::TAP || cfg.gw_mac.isEmpty())
    {
        runPairingMode(cfg);
    }

    Serial.printf("Config: node %s (%s), gateway %s, SSID %s\n",
                  cfg.node_id.c_str(), cfg.node_name.c_str(),
                  cfg.gw_mac.c_str(), cfg.wifi_ssid.c_str());
    return cfg;
}
