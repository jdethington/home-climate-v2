![Tests](https://github.com/jdethington/home-climate-v2/actions/workflows/tests.yml/badge.svg)

# home-climate-v2

Home climate monitoring, rebuilt from scratch: an ESP32 sensor network reporting
over ESP-NOW to a gateway, with a Raspberry Pi as the brain — MQTT broker,
recommendation engine, SQLite history, dashboard, remote access, and phone alerts.

v1 (Wi-Fi HTTP polling, LAN-only dashboard) lives at
[jdethington/home-climate-esp32](https://github.com/jdethington/home-climate-esp32)
and is untouched by this repo.

## Architecture

```
[DHT22 sensor nodes] --ESP-NOW--> [gateway ESP32] --MQTT--> [Raspberry Pi 2B]
  deep sleep, 5-min cycle          always on                 Mosquitto broker
  MAC-ACK + retries                ESP-NOW -> MQTT bridge    Python brain (dew-point logic)
  battery voltage in payload       RGB/LED local display     SQLite (WAL) history
  fw_version in payload            downlink: Pi -> node cmds  dashboard + Cloudflare Tunnel
                                                             ntfy.sh phone alerts
                                                             NWS dust-storm polling
```

Nodes are dumb pipes: sense, send, sleep. All thresholds, hysteresis, and
decision logic live on the Pi in Python, so tuning is a config edit, not a reflash.

## Gateway LEDs + sensor

The gateway runs on the v1 hub's breadboard with the same wiring, and has
its own indication lights plus a DHT11 sensor (GPIO 4).

LEDs (all on the gateway board):

- WiFi (GPIO 16) — on when connected to the AP.
- RGB (GPIO 13/14/18, common-cathode, PWM) — green (0,180,0) when the
  Pi's verdict is OPEN, red (200,0,0) when CLOSED. Driven from the
  `homeclimate/recommendation` MQTT topic.
- Indoor (GPIO 19) and outdoor (GPIO 25) — PWM brightness by data
  freshness: full under 5 min, 2/3 under 10 min, 1/3 under 15 min, off
  after 15 min (nodes report every 5 min, so each missed cycle drops a
  level).
- RX (GPIO 2, onboard) — blinks 150 ms on each ESP-NOW packet received.

The gateway's own sensor reports as node `indoor0` (location "backdoor")
every 5 min via MQTT, like a node — `fw_version` is `"gateway"` and
`batt_v` is null (mains-powered). It counts toward the indoor zone
average (the recommender matches the `indoor` prefix). The dashboard
shows "Gateway" in place of a firmware update button for it; the gateway
itself updates over ElegantOTA at `http://<gateway-ip>/update`.

## MQTT topics

- `homeclimate/sensor/<node-id>` — node readings (JSON from gateway)
- `homeclimate/recommendation` — current OPEN/CLOSED verdict + reasons array
- `homeclimate/cmd/<node-id>` — downlink commands (e.g. `{"ota": true}`)
- `homeclimate/status/<node-id>` — node status (e.g. `ota_ready`)
- `homeclimate/alerts` — current active dust alerts (retained JSON)

## Node payload (ESP-NOW binary struct)

nodeId, location, sensorModel, fw_version, tempC, humidity, battV, bootCount.

## Product direction

Built for home use, architected for product: NVS-based config, captive-portal
provisioning, button-press pairing, and version-checked OTA are first-class
requirements. See issues labeled `product`.

## Layout (planned)

- `firmware/node/` — ESP-NOW sensor node (PlatformIO)
- `firmware/gateway/` — ESP-NOW to MQTT bridge (PlatformIO)
- `pi/` — Python services, dashboard, config
- `docs/` — architecture notes, walk-test results
