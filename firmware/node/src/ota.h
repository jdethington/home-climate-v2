#pragma once
#include "config.h"

// OTA mode (issue #18): join home Wi-Fi, announce ota_ready to the
// gateway (with our IP, so the dashboard can link to the upload page),
// then serve ElegantOTA for 10 minutes. Never returns — reboots on
// timeout, on Wi-Fi failure, or after a successful upload (ElegantOTA
// auto-reboots). Called from setup() instead of sleeping.
void runOtaMode(const NodeConfig &cfg);
