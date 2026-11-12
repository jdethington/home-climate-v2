#include "portal.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "config.h"

#define PORTAL_AP_SSID "ClimateNode-Setup"
#define PORTAL_AP_PASSWORD "climate-setup" // WPA2, min 8 chars; fixed default for now
#define DNS_PORT 53
#define SETUP_BUTTON_PIN 0 // NodeMCU BOOT button; also a strapping pin — never hold across reset
#define LONG_PRESS_MS 3000

static DNSServer dnsServer;
static WebServer server(80);

// MAC format check: AA:BB:CC:DD:EE:FF (hex pairs, any case).
static bool validMac(const String &s)
{
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
        return false;
    for (int i = 0; i < 6; i++)
        if (v[i] < 0 || v[i] > 255)
            return false;
    return true;
}

#define PRESS_WINDOW_MS 1000 // watch window at startup (doubles as serial settle)

PressType detectButtonPress()
{
    pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
    // Phase 1: watch for a press for up to 1 s.
    unsigned long windowStart = millis();
    while (millis() - windowStart < PRESS_WINDOW_MS)
    {
        if (digitalRead(SETUP_BUTTON_PIN) == LOW)
            break; // pressed — time the hold
        delay(10);
    }
    if (digitalRead(SETUP_BUTTON_PIN) == HIGH)
        return PressType::NONE; // never pressed in the window
    // Phase 2: time the hold from the press.
    Serial.println("BOOT pressed — timing the hold...");
    unsigned long pressStart = millis();
    while (millis() - pressStart < LONG_PRESS_MS)
    {
        if (digitalRead(SETUP_BUTTON_PIN) == HIGH)
        {
            if (millis() - pressStart < 50)
                return PressType::NONE; // too short — bounce, not a press
            return PressType::TAP;
        }
        delay(10);
    }
    Serial.println("Long press detected — starting setup portal");
    return PressType::LONG;
}

// True if s is non-empty and every character is a digit.
static bool allDigits(const String &s)
{
    if (s.length() == 0)
        return false;
    for (unsigned int i = 0; i < s.length(); i++)
        if (!isDigit(s[i]))
            return false;
    return true;
}

static String formPage(const NodeConfig &cfg, const String &error)
{
    // Split the stored node_id back into prefix + number for the two fields.
    String prefix = "indoor", number = "";
    if (cfg.node_id.startsWith("indoor"))
    {
        prefix = "indoor";
        number = cfg.node_id.substring(6);
    }
    else if (cfg.node_id.startsWith("outdoor"))
    {
        prefix = "outdoor";
        number = cfg.node_id.substring(7);
    }
    String indoorSel = (prefix == "indoor") ? " selected" : "";
    String outdoorSel = (prefix == "outdoor") ? " selected" : "";

    String html = "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<title>Climate Node Setup</title>"
                  "<style>body{font-family:system-ui,sans-serif;max-width:480px;margin:2rem auto;padding:0 1rem}"
                  "label{display:block;margin:1rem 0 .25rem}input,select{width:100%;padding:.5rem;font-size:1rem;box-sizing:border-box}"
                  "input[type=checkbox]{width:auto}button{margin-top:1.5rem;padding:.75rem 1.5rem;font-size:1rem}.err{color:#b00}</style></head><body>"
                  "<h1>Climate Node Setup</h1>";
    if (error.length() > 0)
        html += "<p class=\"err\">" + error + "</p>";
    html += "<form method=\"POST\" action=\"/save\">"
            "<label>Wi-Fi network name<input name=\"wifi_ssid\" value=\"" +
            cfg.wifi_ssid + "\" required></label>"
                            "<label>Wi-Fi password<input type=\"password\" id=\"wifi_pass\" name=\"wifi_pass\" required></label>"
                            "<label><input type=\"checkbox\" onclick=\"document.getElementById('wifi_pass').type = this.checked ? 'text' : 'password'\"> Show password</label>"
                            "<label>Gateway MAC<input name=\"gw_mac\" value=\"" +
            cfg.gw_mac + "\" placeholder=\"AA:BB:CC:DD:EE:FF\" required></label>"
                         "<label>Node type<select name=\"node_prefix\">";
    html += "<option value=\"indoor\"";
    html += indoorSel;
    html += ">indoor</option>";
    html += "<option value=\"outdoor\"";
    html += outdoorSel;
    html += ">outdoor</option></select></label>";
    html += "<label>Node number<input name=\"node_number\" inputmode=\"numeric\" value=\"" + number + "\" placeholder=\"1\" required></label>"
                                                                                                      "<label>Node name / room<input name=\"node_name\" value=\"" +
            cfg.node_name + "\" placeholder=\"Back porch\" required></label>"
                            "<button type=\"submit\">Save &amp; reboot</button></form></body></html>";
    return html;
}

static void handleRoot()
{
    server.send(200, "text/html", formPage(readStoredConfig(), ""));
}

static void handleSave()
{
    NodeConfig cfg;
    cfg.wifi_ssid = server.arg("wifi_ssid");
    cfg.wifi_pass = server.arg("wifi_pass");
    cfg.gw_mac = server.arg("gw_mac");
    String nodePrefix = server.arg("node_prefix");
    String nodeNumber = server.arg("node_number");
    cfg.node_name = server.arg("node_name");
    cfg.wifi_ssid.trim();
    cfg.wifi_pass.trim();
    cfg.gw_mac.trim();
    nodePrefix.trim();
    nodeNumber.trim();
    cfg.node_name.trim();
    cfg.node_id = nodePrefix + nodeNumber;

    String error;
    if (cfg.wifi_ssid.length() == 0)
        error = "Wi-Fi network name is required.";
    else if (cfg.wifi_pass.length() == 0)
        error = "Wi-Fi password is required.";
    else if (!validMac(cfg.gw_mac))
        error = "Gateway MAC must look like AA:BB:CC:DD:EE:FF.";
    else if (nodePrefix != "indoor" && nodePrefix != "outdoor")
        error = "Node type must be indoor or outdoor.";
    else if (!allDigits(nodeNumber))
        error = "Node number must be digits only.";
    else if (cfg.node_name.length() == 0)
        error = "Node name / room is required.";

    if (error.length() > 0)
    {
        server.send(200, "text/html", formPage(cfg, error));
        return;
    }
    saveConfig(cfg);
    server.send(200, "text/html", "<!doctype html><html><body><h1>Saved — rebooting…</h1><p>Your node will join the network in a few seconds.</p></body></html>");
    delay(1000); // let the response flush
    ESP.restart();
}

void runProvisioningPortal()
{
    Serial.println("Starting setup AP ClimateNode-Setup...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(PORTAL_AP_SSID, PORTAL_AP_PASSWORD);
    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("Setup AP up at %s — join it and open any page\n", apIP.toString().c_str());

    // Captive portal: every DNS lookup resolves to us, so the phone's
    // captive-portal detector pops the setup page automatically.
    dnsServer.start(DNS_PORT, "*", apIP);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    // Anything else redirects to the form (captive-portal detection
    // probes random URLs; this catches them all).
    server.onNotFound([]()
                      {
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", ""); });
    server.begin();

    // Stay awake serving the portal until config is saved (handleSave reboots).
    while (true)
    {
        dnsServer.processNextRequest();
        server.handleClient();
        delay(10);
    }
}
