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
- Indoor (GPIO 19) and outdoor (GPIO 25) — per-node freshness tracking.
  The gateway tracks each node individually (up to 8), so one silent node
  can't hide behind another in the same zone that still reports. A node
  is stale if it hasn't reported within 6 min (one 5-min cycle + 1 min
  of slack):
  - All nodes in the zone fresh → LED at full brightness.
  - A node stale → LED dims to 1/4 brightness, and once per minute it
    blinks the missing node number at full brightness (two blinks = node 2).
  - A node reporting with a broken sensor (NaN readings) → fast
    double-blink (two 150 ms flashes, 1 s pause, repeating) at full
    brightness. Sensor-dead takes priority over the stale blink.
  - The gateway's own sensor (`indoor0`) never blinks a number — if it
    fails, that's a sensor failure, shown by the double-blink.
  - Un-named (provisional `node-XXXX`) nodes aren't tracked until named.
- RX (GPIO 2, onboard) — blinks 150 ms on each ESP-NOW packet received.

The known-node list is stored in the gateway's NVS (namespace `climate`),
so it survives a reboot — loaded nodes get a 6-min grace window to
report. Stale nodes are never removed automatically: the dashboard's
Remove button hides the node (its readings stay logged, and removed
nodes are excluded from the recommendation averages) and tells the
gateway to forget it via `homeclimate/gateway/cmd`. A removed node that
reports again is re-added automatically, on both sides.

The gateway's own sensor reports as node `indoor0` (location "backdoor")
every 5 min via MQTT, like a node — `fw_version` is `"gateway"` and
`batt_v` is null (mains-powered). It counts toward the indoor zone
average (the recommender matches the `indoor` prefix). The dashboard
shows "Gateway" in place of a firmware update button for it; the gateway
itself updates over ElegantOTA at `http://<gateway-ip>/update`.

## Health monitoring (#54)

If the Pi, Mosquitto, or the gateway dies, the dashboard just goes
stale — nobody is told. The health checker fixes that.

`pi/healthcheck.py` runs every 5 minutes from a systemd timer
(`homeclimate-healthcheck.service` + `homeclimate-healthcheck.timer`)
and asks the database when each node last reported. A node silent
longer than `health.silence_minutes` (30) triggers an ntfy alert:

- **New episode** — "system down" if no node has reported, or "node
  silent" naming the silent nodes. Health alerts bypass quiet hours.
- **Still silent** — re-alerts every hour (`health.realert_hours`),
  or every 24 hours (`health.realert_after_ack_hours`) once the
  episode is acknowledged.
- **Recovered** — one alert when readings resume.

Acknowledging is a button on the dashboard: when an episode is active
the dashboard shows a red banner naming the silent nodes, with an
Acknowledge button (POST `/api/acknowledge`). It also works from the
command line: `python healthcheck.py --acknowledge`.

State lives in `pi/health_state.json` (gitignored): whether an episode
is active, when it started, which nodes are silent, when the last
alert went out, and when the next one is due. If an alert fails to
send (no topic, or ntfy unreachable) the state is left untouched, so
the next run retries it.

Hidden nodes (removed from the dashboard) are excluded — a removed
node never reports again, so flagging it would be a permanent false
alarm.

The timer service needs the ntfy topic, like the logging service —
add it with `sudo systemctl edit homeclimate-healthcheck`:

```ini
[Service]
Environment=NTFY_TOPIC=<same value as the homeclimate override>
```

## MQTT topics

- `homeclimate/sensor/<node-id>` — node readings (JSON from gateway)
- `homeclimate/recommendation` — current OPEN/CLOSED verdict + reasons array
- `homeclimate/cmd/<node-id>` — downlink commands (e.g. `{"ota": true}`)
- `homeclimate/status/<node-id>` — node status (e.g. `ota_ready`)
- `homeclimate/alerts` — current active dust alerts (retained JSON)
- `homeclimate/gateway/cmd` — commands for the gateway itself (e.g. `{"forget": "<node-id>"}`)
- `homeclimate/pairing` — node pairing announcements (issue #21)


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
