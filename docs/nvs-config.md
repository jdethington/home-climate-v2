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
| node_id   | String | indoor1               | node          |
| node_name | String | Back porch            | node          |

Limits: NVS namespaces and keys are max 15 characters; all keys above fit.

## Provisioning (captive portal, #20)

First boot (empty NVS) or a long-press of BOOT starts the setup portal:

1. The device starts a `ClimateNode-Setup` Wi-Fi network (WPA2, password `climate-setup`).
2. Join it from a phone or laptop — the setup page opens automatically (or browse to 192.168.4.1).
3. Fill in the form (Wi-Fi credentials, gateway MAC, node type/number, room for a node; Wi-Fi only for the gateway) and submit.
4. The device saves the config to NVS and reboots into normal operation.

Long-press: press EN, then press and hold BOOT for 3 seconds. (Hold it _after_ the board boots — GPIO 0 is a strapping pin, so holding it across the reset lands the chip in download mode instead.) The setup form comes up pre-filled with the current values, except the Wi-Fi password, which is never shown and must be re-entered.

## Not in NVS

- MQTT broker address (192.168.0.104) — a define in the gateway; nodes never use MQTT.
- ESP-NOW channel (6) — a define for now; the gateway's Wi-Fi channel and the nodes' ESP-NOW channel must match. The pairing flow (#21) may handle this later.
- Thresholds, hysteresis, dew-point margins — Pi-side YAML config, never firmware.
