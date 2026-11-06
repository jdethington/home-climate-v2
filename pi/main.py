# connect, subscribe, print
from datetime import datetime

import yaml
import paho.mqtt.client as mqtt

def load_config(path="config.yaml"):
    with open(path) as f:
        return yaml.safe_load(f)

def on_connect(client, userdata, flags, reason_code, properties=None):
    print(f"Connected to broker: {reason_code}")
    for topic in userdata["topics"]:
        client.subscribe(topic)
        print(f"Subscribed to topic: {topic}")

def on_message(client, userdata, msg):
    stamp = datetime.now().isoformat(timespec='seconds')
    print(f"{stamp} {msg.topic}: {msg.payload.decode()}")

def main():
    config = load_config()
    mqtt_config = config["mqtt"]

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.user_data_set({"topics": mqtt_config["topics"]})
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(mqtt_config["host"], mqtt_config["port"])
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("Stopping...")
        client.disconnect()

if __name__ == "__main__":
    main()