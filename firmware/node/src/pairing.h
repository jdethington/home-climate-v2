#pragma once
#include "config.h"

// Pairing mode (issue #21): listen for the gateway beacon, store the
// gateway MAC in NVS, announce to the gateway. Updates cfg.gw_mac on
// success. Returns true if paired, false on timeout (the caller keeps
// the old MAC and proceeds normally).
bool runPairingMode(NodeConfig &cfg);
