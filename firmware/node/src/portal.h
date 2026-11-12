#pragma once

// Captive portal provisioning (issue #20): start the ClimateNode-Setup
// AP, serve the setup form, save the submitted config to NVS, reboot.
// Never returns — the node reboots once valid config is saved.
void runProvisioningPortal();

// What the BOOT button did during startup (issue #21): NONE (not touched),
// TAP (pressed and released — pair), LONG (held 3 s — setup portal).
enum class PressType
{
    NONE,
    TAP,
    LONG
};

// Watches BOOT for up to 1 s (the serial settle time — no extra delay to
// the cycle) and classifies the press. Returns immediately when the button
// is never touched.
PressType detectButtonPress();
