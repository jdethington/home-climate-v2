#pragma once

// Captive portal provisioning (issue #20): start the ClimateNode-Setup
// AP, serve the setup form, save the submitted config to NVS, reboot.
// Never returns — the node reboots once valid config is saved.
void runProvisioningPortal();
