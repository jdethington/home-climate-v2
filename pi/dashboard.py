"""Dashboard (issue #13): live status + history graphs, served by Flask.

Reads the SQLite database for status and history; the current
recommendation + reasons come from latest_recommendation, written every
cycle by the logging service (CLOSED with no reasons before the first
reading). No ordering worry — the dashboard's own Database() call runs the migration,
so the table exists whichever service starts first.
History graphs come from the readings table. The Update firmware button
(issue #18) publishes a one-shot {"ota": true} command to
homeclimate/cmd/<node-id> via paho's publish.single — no persistent MQTT
client here; the node answers ota_ready on homeclimate/status/<node-id>
and the dashboard shows the upload link from node_status. Listens on all
interfaces so the page is reachable on the LAN via mDNS (http://climatepi.local:5000).
Firmware hosting (issue #19): /firmware/<file> serves the published node binary + version manifest from pi/firmware/.
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
        return jsonify(
            {"recommendation": verdict, "reasons": reasons, "sensors": sensors, "alerts": alerts, "node_status": node_status, "latest_fw_version": latest_fw}
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
        return jsonify({"ok": True, "node_id": nodeId, "version": manifest["version"]})
    except Exception as e:
        return jsonify({"ok": False, "error": str(e)}), 502

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
