"""SQLite storage (issue #11): readings + events, WAL mode.

The database file itself is gitignored (*.db in .gitignore) — only the
schema (this file) lives in the repo. Timestamps are stamped here at
ingestion, in UTC ISO format: never trust a deep-sleep node's clock
(the nodes do not even send one).
"""

import sqlite3
from datetime import datetime, timezone


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

    def close(self):
        self.conn.close()
        