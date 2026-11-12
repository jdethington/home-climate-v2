#include "portal.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "gwconfig.h"

#define PORTAL_AP_SSID "ClimateNode-Setup"
#define PORTAL_AP_PASSWORD "climate-setup" // WPA2, min 8 chars; fixed default for now
#define DNS_PORT 53
#define SETUP_BOOT_PIN 0 // NodeMCU BOOT button; also a strapping pin — never hold across reset
#define LONG_PRESS_MS 3000

static DNSServer dnsServer;
static WebServer server(80);

bool longPressDetected()
{
    pinMode(SETUP_BOOT_PIN, INPUT_PULLUP);
    if (digitalRead(SETUP_BOOT_PIN) == HIGH)
        return false; // not pressed — no delay to the normal cycle
    // Button is down — confirm it's a deliberate 3 s hold, not a bump.
    Serial.println("BOOT held — confirming 3 s long press...");
    unsigned long start = millis();
    while (millis() - start < LONG_PRESS_MS)
    {
        if (digitalRead(SETUP_BOOT_PIN) == HIGH)
        {
            Serial.println("BOOT released early — continuing normally");
            return false;
        }
        delay(50);
    }
    Serial.println("Long press detected — starting setup portal");
    return true;
}

static String formPage(const GatewayConfig &cfg, const String &error)
{
    String html = "<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<title>Climate Gateway Setup</title>"
                  "<style>body{font-family:system-ui,sans-serif;max-width:480px;margin:2rem auto;padding:0 1rem}"
                  "label{display:block;margin:1rem 0 .25rem}input{width:100%;padding:.5rem;font-size:1rem;box-sizing:border-box}"
                  "input[type=checkbox]{width:auto}button{margin-top:1.5rem;padding:.75rem 1.5rem;font-size:1rem}.err{color:#b00}</style></head><body>"
                  "<h1>Climate Gateway Setup</h1>";
    if (error.length() > 0)
        html += "<p class=\"err\">" + error + "</p>";
    html += "<form method=\"POST\" action=\"/save\">"
            "<label>Wi-Fi network name<input name=\"wifi_ssid\" value=\"" +
            cfg.wifi_ssid + "\" required></label>"
                            "<label>Wi-Fi password<input type=\"password\" id=\"wifi_pass\" name=\"wifi_pass\" required></label>"
                            "<label><input type=\"checkbox\" onclick=\"document.getElementById('wifi_pass').type = this.checked ? 'text' : 'password'\"> Show password</label>"
                            "<button type=\"submit\">Save &amp; reboot</button></form></body></html>";
    return html;
}

static void handleRoot()
{
    server.send(200, "text/html", formPage(readStoredGatewayConfig(), ""));
}

static void handleSave()
{
    GatewayConfig cfg;
    cfg.wifi_ssid = server.arg("wifi_ssid");
    cfg.wifi_pass = server.arg("wifi_pass");
    cfg.wifi_ssid.trim();
    cfg.wifi_pass.trim();

    String error;
    if (cfg.wifi_ssid.length() == 0)
        error = "Wi-Fi network name is required.";
    else if (cfg.wifi_pass.length() == 0)
        error = "Wi-Fi password is required.";

    if (error.length() > 0)
    {
        server.send(200, "text/html", formPage(cfg, error));
        return;
    }
    saveGatewayConfig(cfg);
    server.send(200, "text/html", "<!doctype html><html><body><h1>Saved — rebooting…</h1><p>Your gateway will join the network in a few seconds.</p></body></html>");
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
