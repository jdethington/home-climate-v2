#pragma once
#include "config.h"

// OTA mode (issue #19): join home Wi-Fi, announce ota_ready to the
// gateway (the gateway clears the queued command on it, so it won't
// re-fire after we reboot), then download the firmware from url with
// HTTPUpdate and reboot into it. Never returns — reboots on Wi-Fi
// failure, on download failure, or after a successful update
// (HTTPUpdate auto-reboots). Called from setup() instead of sleeping.
void runOtaMode(const NodeConfig &cfg, const char *url);
