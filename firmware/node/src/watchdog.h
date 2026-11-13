#pragma once

// Hardware task watchdog (issue #23): if a cycle hangs (sensor read,
// ESP-NOW send, OTA download), the watchdog reboots the node instead
// of draining the battery in a stuck awake state. Timeout is 30 s —
// well above the worst-case normal cycle (~10 s); every longer
// operation (pairing listen, portal, OTA download) feeds it.
void watchdogSetup();

// Feed the watchdog (call from long loops).
void watchdogFeed();
