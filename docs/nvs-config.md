# NVS config (issue #6)

All device configuration lives in NVS under the `climate` namespace,
accessed via the Arduino Preferences library. No credentials or MACs are
hardcoded in firmware.

## Keys

| Key       | Type   | Example               | Used by       |
| --------- | ------ | --------------------- | ------------- |
| wifi_ssid | String | Brimhall22            | node, gateway |
| wifi_pass | String | (your Wi-Fi password) | node, gateway |
| gw_mac    | String | AA:BB:CC:DD:EE:FF     | node          |
| node_id   | String | indoor1               | node, gateway |
| node_name | String | Back porch            | node, gateway |

Limits: NVS namespaces and keys are max 15 characters; all keys above fit.

## Provisioning (Phase 2, temporary)

Before the captive portal (#20) exists, provision over serial:

1. Flash the firmware, open Serial Monitor (115200).
2. On first boot with empty NVS, the firmware prompts for values.
3. Send one `key=value` line per key (e.g. `wifi_ssid=Brimhall22`).
4. The firmware stores each value and reboots into normal operation.

## Not in NVS

- MQTT broker address (192.168.0.104) — a define in the gateway; nodes never use MQTT.
- ESP-NOW channel (1) — a define for now; the gateway's Wi-Fi channel and the nodes' ESP-NOW channel must match. The pairing flow (#21) may handle this later.
- Thresholds, hysteresis, dew-point margins — Pi-side YAML config, never firmware.
