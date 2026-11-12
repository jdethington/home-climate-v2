"""SQLite storage (issue #11): readings, events, latest_recommendation,
nws_alerts, node_status, paired_devices. WAL mode.

The database file itself is gitignored (*.db in .gitignore) — only the
schema (this file) lives in the repo. Timestamps are stamped here at
ingestion, in UTC ISO format: never trust a deep-sleep node's clock
(the nodes do not even send one).
"""

import sqlite3
from datetime import datetime, timezone

import json


# Migrations run in order on every startup; each one only runs if the
# stored version is below its number, so re-running is safe.
MIGRATIONS = {
    1: [
        """CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id TEXT NOT NULL,
            ts TEXT NOT NULL,
            temp_c REAL,
            humidity REAL,
            batt_v REAL,
            fw_version TEXT
        )""",
        """CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ts TEXT NOT NULL,
            type TEXT NOT NULL,
            detail TEXT
        )""",
        # Per-sensor history is the dashboard's main query, so index it.
        "CREATE INDEX IF NOT EXISTS idx_readings_node_ts ON readings (node_id, ts)",
        "CREATE INDEX IF NOT EXISTS idx_events_ts ON events (ts)",
    ],

    2: [
        # One row, id = 1: the latest verdict + reasons, rewritten every
        # cycle. The dashboard reads this; the events table keeps only flips.
        """CREATE TABLE IF NOT EXISTS latest_recommendation (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            ts TEXT NOT NULL,
            recommendation TEXT NOT NULL,
            reasons TEXT NOT NULL
        )""",
    ],

    3: [
        # The current active dust-storm alerts, replaced whole on every
        # poll. Empty table = no active alerts. fetched_at shows staleness
        # when a poll fails and the old rows are kept.
        """CREATE TABLE IF NOT EXISTS nws_alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event TEXT NOT NULL,
            headline TEXT,
            severity TEXT,
            onset TEXT,
            expires TEXT,
            fetched_at TEXT NOT NULL
        )""",
    ],

    4: [
        # Latest status per node (issue #18): ota_ready + IP for the
        # dashboard's upload link. One row per node, replaced on every
        # status message. ts shows staleness — an ota_ready older than
        # the node's 10-min OTA window is dead.
        """CREATE TABLE IF NOT EXISTS node_status (
            node_id TEXT PRIMARY KEY,
            status TEXT NOT NULL,
            ip TEXT,
            ts TEXT NOT NULL
        )""",
    ],

    5: [
        # Paired devices awaiting a name (issue #21): one row per device
        # MAC. node_id is empty or provisional ("node-XXXX") until the
        # user names it from the dashboard; the row is deleted once the
        # rename is issued. A re-pair re-creates it.
        """CREATE TABLE IF NOT EXISTS paired_devices (
            mac TEXT PRIMARY KEY,
            node_id TEXT NOT NULL,
            first_seen TEXT NOT NULL
        )""",
    ],
}

def _now_iso():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")

class Database:
    def __init__(self, path):
        self.conn = sqlite3.connect(path)
        # WAL: readers never block the writer, and a crash mid-write
        # cannot corrupt the file the way a rollback journal can.
        # NORMAL synchronous is the recommended companion to WAL.
        self.conn.execute("PRAGMA journal_mode=WAL")
        self.conn.execute("PRAGMA synchronous=NORMAL")
        self._migrate()

    def _migrate(self):
        self.conn.execute(
            "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER)"
        )
        row = self.conn.execute("SELECT version FROM schema_version").fetchone()
        current = row[0] if row else 0
        for version in sorted(MIGRATIONS):
            if version <= current:
                continue
            for sql in MIGRATIONS[version]:
                self.conn.execute(sql)
            if row is None:
                self.conn.execute(
                    "INSERT INTO schema_version (version) VALUES (?)", (version,)
                )
                row = (version,)
            else:
                self.conn.execute(
                    "UPDATE schema_version SET version = ?", (version,)
                )
            self.conn.commit()
            current = version

    @property
    def version(self):
        row = self.conn.execute("SELECT version FROM schema_version").fetchone()
        return row[0] if row else 0

    def insert_reading(self, node_id, temp_c, humidity, batt_v, fw_version):
        self.conn.execute(
            """INSERT INTO readings (node_id, ts, temp_c, humidity, batt_v, fw_version) 
            VALUES (?, ?, ?, ?, ?, ?)""",
            (node_id, _now_iso(), temp_c, humidity, batt_v, fw_version),
        )
        self.conn.commit()

    def insert_event(self, type, detail):
        self.conn.execute(
            """INSERT INTO events (ts, type, detail) VALUES (?, ?, ?)""",
            (_now_iso(), type, detail),
        )
        self.conn.commit()

    def set_latest_recommendation(self, recommendation, reasons):
        self.conn.execute(
            """INSERT OR REPLACE INTO latest_recommendation
            (id, ts, recommendation, reasons) VALUES (1, ?, ?, ?)""",
            (_now_iso(), recommendation, json.dumps(reasons)),
        )
        self.conn.commit()

    def get_latest_recommendation(self):
        row = self.conn.execute(
            "SELECT recommendation, reasons FROM latest_recommendation WHERE id = 1"
        ).fetchone()
        if row is None:
            return "CLOSED", []
        return row[0], json.loads(row[1])

    def set_nws_alerts(self, alerts):
        self.conn.execute("DELETE FROM nws_alerts")
        self.conn.executemany(
            """INSERT INTO nws_alerts (event, headline, severity, onset, expires, fetched_at)
                VALUES (?, ?, ?, ?, ?, ?)""",
            [(a["event"], a["headline"], a["severity"], a["onset"], a["expires"], _now_iso()) for a in alerts],
        )
        self.conn.commit()

    def get_nws_alerts(self):
        rows = self.conn.execute(
            "SELECT event, headline, severity, onset, expires FROM nws_alerts ORDER BY onset"
        ).fetchall()
        return [
            {"event": r[0], "headline": r[1], "severity": r[2], "onset": r[3], "expires": r[4]}
            for r in rows
        ]

    def set_node_status(self, node_id, status, ip):
        self.conn.execute(
            """INSERT OR REPLACE INTO node_status
            (node_id, status, ip, ts) VALUES (?, ?, ?, ?)""",
            (node_id, status, ip, _now_iso()),
        )
        self.conn.commit()

    def get_node_status(self):
        rows = self.conn.execute(
            "SELECT node_id, status, ip, ts FROM node_status ORDER BY node_id"
        ).fetchall()
        return [
            {"node_id": r[0], "status": r[1], "ip": r[2], "ts": r[3]}
            for r in rows
        ]
    
    def record_pairing(self, mac, node_id):
        self.conn.execute(
            """INSERT INTO paired_devices (mac, node_id, first_seen)
            VALUES (?, ?, ?)
            ON CONFLICT(mac) DO UPDATE SET node_id = excluded.node_id""",
            (mac, node_id, _now_iso()),
        )
        self.conn.commit()

    def get_unnamed_devices(self):
        # Un-named = empty node_id, or a provisional "node-XXXX" id
        # (generated from the MAC — module 4). Named nodes never appear.
        rows = self.conn.execute(
            """SELECT mac, node_id, first_seen FROM paired_devices
            WHERE node_id = '' OR node_id LIKE 'node-%' ORDER BY first_seen"""
        ).fetchall()
        return [
            {"mac": r[0], "node_id": r[1], "first_seen": r[2]}
            for r in rows
        ]

    def mark_device_named(self, mac):
        self.conn.execute("DELETE FROM paired_devices WHERE mac = ?", (mac,))
        self.conn.commit()

    def clear_node_status(self, node_id):
        # A node sending readings is definitively out of OTA mode.
        self.conn.execute(
            "DELETE FROM node_status WHERE node_id = ?", (node_id,)
        )
        self.conn.commit()

    def close(self):
        self.conn.close()
        