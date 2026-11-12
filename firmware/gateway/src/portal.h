#pragma once

// Captive portal provisioning (issue #20): start the ClimateNode-Setup
// AP, serve the setup form, save the submitted Wi-Fi credentials to NVS,
// reboot. Never returns — the gateway reboots once valid config is saved.
void runProvisioningPortal();

// True if BOOT (GPIO 0) is held down for a full 3 seconds. Returns
// immediately (no delay to the normal cycle) when the button is up.
bool longPressDetected();
