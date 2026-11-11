"""Logging service (issue #12): MQTT -> SQLite + recommendation.

Subscribes to homeclimate/sensor/#, stores every reading, recomputes
the recommendation each cycle, publishes homeclimate/recommendation,
and logs OPEN/CLOSED flips to the events table. Runs as a systemd
service (pi/homeclimate.service).
"""

import json
import time
from datetime import datetime

import yaml
import paho.mqtt.client as mqtt

from database import Database
from recommendation import Recommender

def load_config(path="config.yaml"):
    with open(path) as f:
        return yaml.safe_load(f)

def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"Connected to broker: {reason_code}")
    for topic in userdata["topics"]:
        client.subscribe(topic)
        print(f"Subscribed to topic: {topic}")

def on_message(client, userdata, msg):
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
    userdata["recommender"].update(
        node_id, payload.get("temp_c"), payload.get("humidity")
    )
    verdict = userdata["recommender"].recommend()

    if verdict != userdata["last_verdict"]:
        userdata["db"].insert_event(
            "recommendation", f"{userdata['last_verdict']} -> {verdict}"
        )
        print(f"Recommendation flipped: {userdata['last_verdict']} -> {verdict}")
        userdata["last_verdict"] = verdict

    # Retained: the verdict is state, not an event — a late subscriber
    # (dashboard, future services) should see the current verdict
    # immediately instead of waiting for the next reading.
    client.publish(
        "homeclimate/recommendation", 
        json.dumps({"recommendation": verdict}),
        retain=True
    )
    stamp = datetime.now().isoformat(timespec='seconds')
    print(f"{stamp} {msg.topic}: {msg.payload.decode()} -> {verdict}")

def main():
    config = load_config()
    mqtt_config = config["mqtt"]
    db = Database(config["database"]["path"])
    recommender = Recommender(config)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.user_data_set({"topics": mqtt_config["topics"], "db": db, "recommender": recommender, "last_verdict": "CLOSED"})
    client.on_connect = on_connect
    client.on_message = on_message

    # The broker may not be up yet when this service starts — retry
    # instead of crashing and letting systemd restart us in a loop.

    while True:
        try:
            client.connect(mqtt_config["host"], mqtt_config["port"])
            break
        except OSError as e:
            print(f"Broker not available: {e}. Retrying in 5 seconds...")
            time.sleep(5)

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("Stopping...")
    finally:
        client.disconnect()
        db.close()

if __name__ == "__main__":
    main()