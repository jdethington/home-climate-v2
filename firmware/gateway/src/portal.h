#pragma once

// Captive portal provisioning (issue #20): start the ClimateNode-Setup
// AP, serve the setup form, save the submitted Wi-Fi credentials to NVS,
// reboot. Never returns — the gateway reboots once valid config is saved.
void runProvisioningPortal();
