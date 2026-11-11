"""Dashboard (issue #13): live status + history graphs, served by Flask.

Reads the SQLite database only — no MQTT client here. The current
recommendation comes from the events table (the latest flip's target,
CLOSED when nothing has flipped yet). Listens on all interfaces so
the page is reachable on the LAN via mDNS (http://climatepi.local:5000).
"""

import sqlite3
import yaml
from flask import Flask, render_template, jsonify, request

from database import Database

def load_config(path="config.yaml"):
    with open(path) as f:
        return yaml.safe_load(f)

app = Flask(__name__)
config = load_config()
DB_PATH = config["database"]["path"]

def current_recommendation(db):
    row = db.conn.execute(
        "SELECT detail FROM events WHERE type = 'recommendation' "
        "ORDER BY id DESC LIMIT 1"
    ).fetchone()
    if row is None:
        return "CLOSED"
    # detail is like "OPEN -> CLOSED" or "CLOSED -> OPEN", so we take the last part
    return row[0].split("->")[-1].strip()

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
        status = [
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
        return jsonify(
            { "recommendation": current_recommendation(db), "sensors": sensors}
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

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=config.get("dashboard", {}).get("port", 5000))
