"""Logging service (issue #12): MQTT -> SQLite + recommendation.

Subscribes to homeclimate/sensor/#, stores every reading, recomputes
the recommendation each cycle, publishes homeclimate/recommendation with a reasons array,
and logs OPEN/CLOSED flips to the events table. Also subscribes to
homeclimate/status/# (issue #18) and stores each node's latest status
(ota_ready + IP) in node_status for the dashboard — status messages are
not readings and never touch the readings table. Runs as a systemd
service (pi/homeclimate.service).
"""

import json
import time
from datetime import datetime

import yaml
import paho.mqtt.client as mqtt

from database import Database
from recommendation import Recommender

from notifier import send_flip, send_notification
import os

import threading
from nws_alerts import fetch_dust_alerts

def load_config(path="config.yaml"):
    with open(path) as f:
        return yaml.safe_load(f)

def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"Connected to broker: {reason_code}")
    for topic in userdata["topics"]:
        client.subscribe(topic)
        print(f"Subscribed to topic: {topic}")

def on_message(client, userdata, msg):
    # Status messages (homeclimate/status/#) are not readings — route them
    # before the sensor path, or they'd land as NULL rows in readings.
    if msg.topic.startswith("homeclimate/status/"):
        handle_status(userdata, msg)
        return

    try:
        payload = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        print(f"Ignoring bad JSON on {msg.topic}")
        return
    node_id = payload.get("node_id")
    if not node_id:
        print(f"Ignoring message without node_id on {msg.topic}")
        return

    userdata["db"].insert_reading(
        node_id,
        payload.get("temp_c"),
        payload.get("humidity"),
        payload.get("batt_v"),
        payload.get("fw_version"),
    )
    userdata["db"].clear_node_status(node_id)
    userdata["recommender"].update(
        node_id, payload.get("temp_c"), payload.get("humidity")
    )
    verdict, reasons = userdata["recommender"].recommend()
    userdata["db"].set_latest_recommendation(verdict, reasons)

    if verdict != userdata["last_verdict"]:
        userdata["db"].insert_event(
            "recommendation", f"{userdata['last_verdict']} -> {verdict}"
        )
        print(f"Recommendation flipped: {userdata['last_verdict']} -> {verdict}")
        userdata["last_verdict"] = verdict

        ntfy_cfg = userdata.get("ntfy", {})
        if ntfy_cfg.get("enabled", True):
            send_flip(
                ntfy_cfg.get("topic", ""),
                verdict,
                reasons,
                ntfy_cfg.get("quiet_start", 22),
                ntfy_cfg.get("quiet_end", 7),
            )
    # Retained: the verdict is state, not an event — a late subscriber
    # (dashboard, future services) should see the current verdict
    # immediately instead of waiting for the next reading.
    client.publish(
        "homeclimate/recommendation",
        json.dumps({"recommendation": verdict, "reasons": reasons}),
        retain=True,
    )
    stamp = datetime.now().isoformat(timespec='seconds')
    print(f"{stamp} {msg.topic}: {msg.payload.decode()} -> {verdict}: {'; '.join(reasons)}")

def nws_poller(db_path, client, ntfy_cfg, nws_cfg, stop_event):
    """Background thread (issue #17): poll NWS for dust alerts.

    Polls every poll_interval_minutes; writes the current active set to
    the nws_alerts table, publishes it retained to homeclimate/alerts, and
    notifies via ntfy when a NEW warning appears — but only if the
    windows are open (verdict OPEN), and never during quiet hours. Owns
    its own Database connection — sqlite3 connections must not be shared
    across threads. paho's publish() is thread-safe.
    """
    db = Database(db_path)
    # Exact house coordinates come from the environment (systemd override)
    # — never from the repo. Falls back to the Gilbert center in config.
    lat = float(os.environ.get("NWS_LATITUDE", nws_cfg.get("latitude", 33.3528)))
    lon = float(os.environ.get("NWS_LONGITUDE", nws_cfg.get("longitude", -111.7890)))
    interval = nws_cfg.get("poll_interval_minutes", 15) * 60
    previous_ids = set()
    first_poll = True
    try:
        while not stop_event.is_set():
            alerts = fetch_dust_alerts(lat, lon)
            if alerts is not None:
                db.set_nws_alerts(alerts)
                client.publish(
                    "homeclimate/alerts",
                    json.dumps({"alerts": alerts}),
                    retain=True,
                )
                print(f"nws: {len(alerts)} active dust alert(s)")
                current_ids = {a["id"] for a in alerts}
                if not first_poll and ntfy_cfg.get("enabled", True):
                    verdict, _ = db.get_latest_recommendation()
                    for alert in alerts:
                        if alert["id"] not in previous_ids and verdict == "OPEN":
                            send_notification(
                                ntfy_cfg.get("topic", ""),
                                alert["event"],
                                alert["headline"] or alert["event"],
                                ntfy_cfg.get("quiet_start", 22),
                                ntfy_cfg.get("quiet_end", 7),
                            )
                previous_ids = current_ids
                first_poll = False
            stop_event.wait(interval)
    finally:
        db.close()

def handle_status(userdata, msg):
    """Node status message (issue #18): {"node_id": ..., "status": "ota_ready",
    "ip": ...}. Stored in node_status for the dashboard; NOT a reading."""
    try:
        payload = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        print(f"Ignoring bad status JSON on {msg.topic}")
        return
    node_id = payload.get("node_id")
    if not node_id:
        print(f"Ignoring status without node_id on {msg.topic}")
        return
    userdata["db"].set_node_status(node_id, payload.get("status", ""), payload.get("ip"))
    print(f"{datetime.now().isoformat(timespec='seconds')} STATUS {node_id}: {payload.get('status')} {payload.get('ip', '')}")

def main():
    config = load_config()
    mqtt_config = config["mqtt"]
    db = Database(config["database"]["path"])
    recommender = Recommender(config)

    ntfy_cfg = config.get("ntfy", {})
    ntfy_cfg["topic"] = os.environ.get("NTFY_TOPIC", "")


    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.user_data_set({"topics": mqtt_config["topics"], "db": db, "recommender": recommender, "last_verdict": "CLOSED"})
    client.on_connect = on_connect
    client.on_message = on_message
    client.user_data_set({"topics": mqtt_config["topics"], "db": db, "recommender": recommender, "last_verdict": "CLOSED", "ntfy": config.get("ntfy", {})})

    # The broker may not be up yet when this service starts — retry
    # instead of crashing and letting systemd restart us in a loop.

    while True:
        try:
            client.connect(mqtt_config["host"], mqtt_config["port"])
            break
        except OSError as e:
            print(f"Broker not available: {e}. Retrying in 5 seconds...")
            time.sleep(5)

    stop_event = threading.Event()
    poller = threading.Thread(
        target=nws_poller,
        args=(config["database"]["path"], client, ntfy_cfg, config.get("nws", {}), stop_event),
        daemon=True,
    )
    poller.start()

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("Stopping...")
    finally:
        stop_event.set()
        client.disconnect()
        db.close()

if __name__ == "__main__":
    main()