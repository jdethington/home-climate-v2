# NVS config (issue #6)

All device configuration lives in NVS under the `climate` namespace,
accessed via the Arduino Preferences library. No credentials or MACs are
hardcoded in firmware.

## Keys

| Key       | Type   | Example               | Used by       |
| --------- | ------ | --------------------- | ------------- |
| wifi_ssid | String | MyHomeNetwork         | node, gateway |
| wifi_pass | String | (your Wi-Fi password) | node, gateway |
| gw_mac    | String | AA:BB:CC:DD:EE:FF     | node          |
| node_id   | String | indoor1               | node          |
| node_name | String | Back porch            | node          |

Limits: NVS namespaces and keys are max 15 characters; all keys above fit.

## Provisioning (captive portal, #20)

First boot (empty NVS) or a long-press of BOOT starts the setup portal:

1. The device starts a `ClimateNode-Setup` Wi-Fi network (WPA2, password `climate-setup`).
2. Join it from a phone or laptop — the setup page opens automatically (or browse to 192.168.4.1).
3. Fill in the form — Wi-Fi credentials are required; the gateway MAC, node number, and room are optional. Leave the MAC blank and the node pairs itself with the gateway on first boot; leave the number/name blank and the node shows up on the dashboard as "unknown node", ready to name there.
4. The device saves the config to NVS and reboots into normal operation.

Long-press: press EN, then press and hold BOOT for 3 seconds. (Hold it _after_ the board boots — GPIO 0 is a strapping pin, so holding it across the reset lands the chip in download mode instead.) The setup form comes up pre-filled with the current values, except the Wi-Fi password, which is never shown and must be re-entered.

## Pairing (button press, #21)

- Tap BOOT at startup: the node listens up to 60 s for the gateway beacon, stores the gateway MAC, and announces itself. Use this to move a node to a different gateway.
- No gateway MAC stored: the node pairs automatically on boot (same listen), and sleeps to retry next cycle if no beacon is heard.
- A node with no name reports under a provisional `node-XXXX` ID (from its MAC) until it is named from the dashboard.

## Not in NVS

- MQTT broker address (192.168.0.104) — a define in the gateway; nodes never use MQTT.
- ESP-NOW channel (6) — a define for now; the gateway's Wi-Fi channel and the nodes' ESP-NOW channel must match. The pairing flow (#21) may handle this later.
- Thresholds, hysteresis, dew-point margins — Pi-side YAML config, never firmware.
