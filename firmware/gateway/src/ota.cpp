#include "ota.h"
#include <WebServer.h>
#include <ElegantOTA.h>

static WebServer server(80);

void otaSetup()
{
    // A bare-IP landing page so a browser to the gateway's address
    // doesn't 404 (ElegantOTA serves only /update).
    server.on("/", []()
              { server.send(200, "text/plain", "Climate gateway — firmware update at /update"); });
    ElegantOTA.begin(&server);
    server.begin();
    Serial.println("OTA update page at http://<gateway-ip>/update");
}

void otaLoop()
{
    server.handleClient();
    ElegantOTA.loop();
}
