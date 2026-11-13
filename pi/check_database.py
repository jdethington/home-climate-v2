"""Quick checks for database.py (issue #11). Run from pi/:
source venv/bin/activate
python check_database.py
"""
import os
from database import Database

DB = "/tmp/test_homeclimate.db"
for suffix in ("", "-wal", "-shm"):
    if os.path.exists(DB + suffix):
        os.remove(DB + suffix)

db = Database(DB)
print("version:", db.version)  # expect 4
print("journal mode:", db.conn.execute("PRAGMA journal_mode").fetchone()[0])  # expect wal

db.insert_reading("indoor1", 27.3, 34, 4.9, "1.0.0")
db.insert_reading("outdoor1", 25.3, 57.1, None, "1.0.0")
db.insert_event("recommendation", "CLOSED -> OPEN")
db.set_latest_recommendation("OPEN", ["outdoor 22.0C is 5.0C cooler than indoor 27.0C"])
db.set_node_status("indoor1", "ota_ready", "192.168.0.131")
print("node status:", db.get_node_status())  # expect [{'node_id': 'indoor1', 'status': 'ota_ready', 'ip': '192.168.0.131', 'ts': ...}]
db.clear_node_status("indoor1")
print("node status after clear:", db.get_node_status())  # expect []

print("latest recommendation:", db.get_latest_recommendation())  # expect ("OPEN", [...])
for row in db.conn.execute(
    "SELECT node_id, ts, temp_c, humidity, batt_v, fw_version FROM readings"
):
    print(row)
print("events:", db.conn.execute("SELECT type, detail FROM events").fetchall())
db.close()

# Re-open: migrate must be a no-op and nothing may duplicate
db = Database(DB)
print("version after reopen:", db.version)  # expect 4
print("readings after reopen:",
      db.conn.execute("SELECT COUNT(*) FROM readings").fetchone()[0])  # expect 2
db.close()
print("OK")
