"""Tests for the backup script (issue #53)."""

import gzip
import sqlite3
from datetime import datetime, timezone

from backup import (
    timestamp_now, backup_database, gzip_file, backup_config,
    upload_to_destination, run_backup,
)

NOW = datetime(2026, 11, 26, 2, 0, tzinfo=timezone.utc)


def make_test_db(path):
    conn = sqlite3.connect(path)
    conn.execute("CREATE TABLE readings (node_id TEXT, temp_c REAL)")
    conn.execute("INSERT INTO readings VALUES ('indoor1', 21.5)")
    conn.execute("INSERT INTO readings VALUES ('outdoor1', 15.0)")
    conn.commit()
    conn.close()


def test_timestamp_now():
    assert timestamp_now(NOW) == "2026-11-26T020000Z"


def test_backup_database_copies_rows(tmp_path):
    src = tmp_path / "src.db"
    make_test_db(str(src))
    dst = tmp_path / "dst.db"
    backup_database(str(src), str(dst))
    conn = sqlite3.connect(str(dst))
    rows = conn.execute("SELECT COUNT(*) FROM readings").fetchone()[0]
    conn.close()
    assert rows == 2


def test_gzip_file_round_trip(tmp_path):
    src = tmp_path / "a.db"
    src.write_bytes(b"hello backup")
    dst = tmp_path / "a.db.gz"
    gzip_file(str(src), str(dst))
    assert not src.exists()
    with gzip.open(str(dst), "rb") as f:
        assert f.read() == b"hello backup"


def test_backup_config(tmp_path):
    src = tmp_path / "config.yaml"
    src.write_text("database:\n  path: homeclimate.db\n")
    dst = tmp_path / "config-2026.yaml"
    backup_config(str(src), str(dst))
    assert dst.read_text() == src.read_text()


def test_upload_to_local_destination(tmp_path):
    src = tmp_path / "homeclimate-2026.db.gz"
    src.write_bytes(b"x")
    dest = tmp_path / "usb"
    dest.mkdir()
    assert upload_to_destination(str(src), str(dest)) is True
    assert (dest / "homeclimate-2026.db.gz").exists()


def test_upload_to_missing_local_destination_skips(tmp_path):
    src = tmp_path / "homeclimate-2026.db.gz"
    src.write_bytes(b"x")
    missing = tmp_path / "not-mounted"
    assert upload_to_destination(str(src), str(missing)) is False
    assert not missing.exists()


def test_upload_to_rclone_without_remote_fails_cleanly(tmp_path):
    src = tmp_path / "homeclimate-2026.db.gz"
    src.write_bytes(b"x")
    assert upload_to_destination(str(src), "rclone:nosuchremote:backups") is False


def test_run_backup_full_cycle(tmp_path):
    db = tmp_path / "homeclimate.db"
    make_test_db(str(db))
    cfg = tmp_path / "config.yaml"
    cfg.write_text("database:\n  path: homeclimate.db\n")
    dest = tmp_path / "usb"
    dest.mkdir()
    config = {
        "database": {"path": str(db)},
        "backup": {
            "local_dir": str(tmp_path / "backups"),
            "destinations": [str(dest)],
        },
    }
    uploaded = run_backup(config, config_path=str(cfg), now=NOW)
    staging = tmp_path / "backups"
    assert (staging / "homeclimate-2026-11-26T020000Z.db.gz").exists()
    assert (staging / "config-2026-11-26T020000Z.yaml").exists()
    assert (dest / "homeclimate-2026-11-26T020000Z.db.gz").exists()
    assert (dest / "config-2026-11-26T020000Z.yaml").exists()
    assert len(uploaded) == 2
