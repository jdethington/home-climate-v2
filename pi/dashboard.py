"""Dashboard (issue #13): live status + history graphs, served by Flask.

Reads the SQLite database for status and history; the current
recommendation + reasons come from latest_recommendation, written every
cycle by the logging service (CLOSED with no reasons before the first
reading). No ordering worry — the dashboard's own Database() call runs the migration,
so the table exists whichever service starts first.
History graphs come from the readings table. 
Version-checked OTA (issue #19): the dashboard compares each node's fw_version against the
published manifest and shows an "Update to <version>" button when one is
available; one click publishes {"ota": true, "url": ...} to
homeclimate/cmd/<node-id> via paho's publish.single — no persistent MQTT
client here; the node downloads the firmware from the URL itself and
answers ota_ready on homeclimate/status/<node-id> while it updates.
"""

import hashlib
import os
import sqlite3
import yaml
import json

from flask import Flask, render_template, jsonify, request, send_from_directory, abort
from database import Database
from paho.mqtt import publish as mqtt_publish

def load_config(path="config.yaml"):
    with open(path) as f:
        return yaml.safe_load(f)

app = Flask(__name__)
config = load_config()

def load_manifest():
    """Read the published firmware manifest (pi/firmware/version.json).

    Returns the manifest dict (version, file, md5) or None if no
    firmware has been published yet.
    """
    path = os.path.join(FIRMWARE_DIR, "version.json")
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None

DB_PATH = config["database"]["path"]

MQTT_HOST = config["mqtt"]["host"]
MQTT_PORT = config["mqtt"]["port"]

FIRMWARE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            config.get("firmware", {}).get("dir", "firmware"))

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/status")
def api_status():
    db = Database(DB_PATH)
    try:
        latest = db.conn.execute(
            """SELECT r.node_id, r.ts, r.temp_c, r.humidity, r.batt_v, r.fw_version
                FROM readings r
                INNER JOIN (
                    SELECT node_id, MAX(ts) AS max_ts
                    FROM readings
                    GROUP BY node_id
                ) m ON r.node_id = m.node_id AND r.ts = m.max_ts
               ORDER BY r.node_id"""
        ).fetchall()
        sensors = [
            {
                "node_id": row[0],
                "ts": row[1],
                "temp_c": row[2],
                "humidity": row[3],
                "batt_v": row[4],
                "fw_version": row[5],
            }
            for row in latest
        ]
        verdict, reasons = db.get_latest_recommendation()
        alerts = db.get_nws_alerts()
        node_status = db.get_node_status()
        manifest = load_manifest()
        latest_fw = manifest["version"] if manifest else None
        new_devices = db.get_unnamed_devices()
        return jsonify(
            {"recommendation": verdict, "reasons": reasons, "sensors": sensors, "alerts": alerts, "node_status": node_status, "latest_fw_version": latest_fw, "new_devices": new_devices}
        )
    finally:
        db.close()

@app.route("/api/history")
def history():
    node_id = request.args.get("node_id")
    hours = int(request.args.get("hours", 24))
    db = Database(DB_PATH)
    try:
        # datetime(ts) normalizes the stored "+00:00" ISO timestamps
        # into SQLite's canonical form before comparing to the cutoff.
        rows = db.conn.execute(
            """SELECT ts, temp_c, humidity FROM readings
               WHERE node_id = ? AND datetime(ts) >= datetime('now', ?)
                ORDER BY ts""",
            (node_id, f"-{hours} hours"),
        ).fetchall()
        return jsonify(
            [{"ts": r[0], "temp_c": r[1], "humidity": r[2]} for r in rows]
        )
    finally:
        db.close()

@app.route("/api/update/<node_id>", methods=["POST"])
def update_node(node_id):
    """Trigger a version-checked OTA update: publish {"ota": true, "url": ...}
    to the node's command topic. The gateway queues it and forwards it on
    the node's next reading (within 5 min); the node downloads the firmware
    from the URL itself. 503 if no firmware has been published yet."""
    manifest = load_manifest()
    if manifest is None:
        return jsonify({"ok": False, "error": "no firmware published"}), 503
    base_url = config.get("firmware", {}).get("base_url", "").rstrip("/")
    url = f"{base_url}/{manifest['file']}"
    payload = json.dumps({"ota": True, "url": url})
    try:
        mqtt_publish.single(
            f"homeclimate/cmd/{node_id}", payload,
            hostname=MQTT_HOST, port=MQTT_PORT,
        )
        return jsonify({"ok": True, "node_id": node_id, "version": manifest["version"]})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 502

@app.route("/api/name", methods=["POST"])
def name_device():
    """Name a newly paired device (issue #21): publish a rename command
    to the device's provisional node_id and drop it from the unnamed list.
    Body: {"mac": ..., "node_id": ..., "node_name": ...}. The node applies
    the rename on its next reading (module 4) and reboots with its new name."""
    data = request.get_json(silent=True) or {}
    mac = (data.get("mac") or "").strip()
    node_id = (data.get("node_id") or "").strip()
    node_name = (data.get("node_name") or "").strip()
    # Validate like the portal: indoor/outdoor prefix + digits-only number.
    ok_id = (
        (node_id.startswith("indoor") and node_id[6:].isdigit())
        or (node_id.startswith("outdoor") and node_id[7:].isdigit())
    )
    if not mac or not ok_id or not node_name:
        return jsonify({"ok": False, "error": "mac, a valid node_id (indoor/outdoor + number), and node_name are required"}), 400
    db = Database(DB_PATH)
    try:
        devices = {d["mac"]: d for d in db.get_unnamed_devices()}
        if mac not in devices:
            return jsonify({"ok": False, "error": "unknown device"}), 404
        provisional_id = devices[mac]["node_id"]
        if not provisional_id:
            return jsonify({"ok": False, "error": "device has no provisional id to address"}), 409
        payload = json.dumps({"rename": {"node_id": node_id, "node_name": node_name}})
        try:
            mqtt_publish.single(
                f"homeclimate/cmd/{provisional_id}", payload,
                hostname=MQTT_HOST, port=MQTT_PORT,
            )
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 502
        db.mark_device_named(mac)
        return jsonify({"ok": True, "node_id": node_id})
    finally:
        db.close()


@app.route("/firmware/<path:filename>")
def firmware_file(filename):
    """Serve a firmware file (binary or manifest) from the firmware dir.

    For .bin files, also sends an X-MD5 header — the node's HTTPUpdate
    library verifies the download against it before flashing.
    """
    path = os.path.join(FIRMWARE_DIR, filename)
    if not os.path.isfile(path):
        abort(404)
    response = send_from_directory(FIRMWARE_DIR, filename)
    if filename.endswith(".bin"):
        with open(path, "rb") as f:
            response.headers["X-MD5"] = hashlib.md5(f.read()).hexdigest()
    return response

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=config.get("dashboard", {}).get("port", 5000))
