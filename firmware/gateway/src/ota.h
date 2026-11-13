#pragma once

// ElegantOTA update server (issue #22): serves the firmware upload
// page at http://<gateway-ip>/update. The gateway is always-on and
// Wi-Fi-connected, so it gets the easy path — no downlink trigger.
void otaSetup();

// Handles web requests; call every loop iteration.
void otaLoop();
