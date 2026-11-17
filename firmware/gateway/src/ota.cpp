#include "ota.h"
#include <WebServer.h>
#include <WiFi.h>
#include <ElegantOTA.h>

static WebServer server(80);

void otaSetup()
{
    // A bare-IP landing page so a browser to the gateway's address
    // doesn't 404 (ElegantOTA serves only /update).
    server.on("/", []()
              { server.send(200, "text/plain", "Climate gateway — OTA build 1.1.1 — firmware update at /update"); });
    ElegantOTA.begin(&server);
    server.begin();
    Serial.printf("OTA update page at http://%s/update\n", WiFi.localIP().toString().c_str());
}

void otaLoop()
{
    server.handleClient();
    ElegantOTA.loop();
}
