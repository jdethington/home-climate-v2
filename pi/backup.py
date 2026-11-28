"""Nightly backup (issue #53): snapshot the database + config, upload
to each configured destination.

The database is snapshotted with SQLite's online backup API, so the
copy is consistent even while the logging service is writing (WAL).
The snapshot is gzipped, the config is copied alongside, and both are
timestamped. Destinations are a config list: rclone remotes
("rclone:drive:homeclimate-backups") for cloud, plain paths
("/media/piadmin/USB/backups") for a local drive. A failed upload
never deletes the local staging copy.
"""

import argparse
import gzip
import os
import shutil
import sqlite3
import subprocess
from datetime import datetime, timezone

import yaml

DEFAULT_CONFIG_PATH = "config.yaml"


def timestamp_now(now=None):
    """UTC timestamp for backup filenames: 2026-11-26T020000Z."""
    now = now or datetime.now(timezone.utc)
    return now.strftime("%Y-%m-%dT%H%M%SZ")


def backup_database(db_path, dest_path):
    """Consistent snapshot of a live SQLite DB (online backup API).

    The source is opened read-only, so a wrong path fails loudly
    instead of creating an empty DB. Works while the logging service
    is writing — no need to stop anything.
    """
    src = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    try:
        if os.path.exists(dest_path):
            os.remove(dest_path)
        dst = sqlite3.connect(dest_path)
        try:
            src.backup(dst)
        finally:
            dst.close()
    finally:
        src.close()


def gzip_file(src_path, dest_path):
    """Compress src_path to dest_path (.gz), removing the original."""
    with open(src_path, "rb") as f_in, gzip.open(dest_path, "wb") as f_out:
        shutil.copyfileobj(f_in, f_out)
    os.remove(src_path)


def backup_config(config_path, dest_path):
    """Copy the config file (tiny, plain copy)."""
    shutil.copy2(config_path, dest_path)


def upload_to_destination(local_path, destination):
    """Upload one file to a destination. Returns True on success.

    A destination starting with "rclone:" is a cloud remote — uploaded
    with `rclone copy` (never deletes on the remote). Anything else is
    a local directory path — plain copy, but only if the directory
    exists: a missing USB mount must skip, not silently copy onto the
    Pi's SD card.
    """
    if destination.startswith("rclone:"):
        remote = destination[len("rclone:"):]
        try:
            result = subprocess.run(
                ["rclone", "copy", local_path, remote],
                capture_output=True, text=True,
            )
        except FileNotFoundError:
            print("backup: rclone not installed — skipping cloud upload")
            return False
        if result.returncode != 0:
            print(f"backup: rclone copy to {remote} failed: "
                  f"{result.stderr.strip()}")
            return False
        return True
    if not os.path.isdir(destination):
        print(f"backup: local destination {destination} not present — skipping")
        return False
    shutil.copy2(local_path, destination)
    return True


def run_backup(config, config_path=DEFAULT_CONFIG_PATH, now=None, dry_run=False):
    """One backup cycle: snapshot, gzip, config copy, upload each file."""
    backup_cfg = config.get("backup", {})
    local_dir = backup_cfg.get("local_dir", "backups")
    db_path = config.get("database", {}).get("path", "homeclimate.db")
    stamp = timestamp_now(now)
    os.makedirs(local_dir, exist_ok=True)
    db_tmp = os.path.join(local_dir, f"homeclimate-{stamp}.db")
    db_gz = db_tmp + ".gz"
    cfg_dest = os.path.join(local_dir, f"config-{stamp}.yaml")
    if dry_run:
        print(f"[dry run] would back up {db_path} -> {db_gz}")
        print(f"[dry run] would copy {config_path} -> {cfg_dest}")
        for dest in backup_cfg.get("destinations", []):
            print(f"[dry run] would upload to {dest}")
        return []
    backup_database(db_path, db_tmp)
    gzip_file(db_tmp, db_gz)
    backup_config(config_path, cfg_dest)
    print(f"backup: {db_gz} + {cfg_dest} staged in {local_dir}")
    uploaded = []
    for dest in backup_cfg.get("destinations", []):
        for path in (db_gz, cfg_dest):
            if upload_to_destination(path, dest):
                uploaded.append((path, dest))
                print(f"backup: uploaded {os.path.basename(path)} -> {dest}")
    return uploaded


def load_config(path=DEFAULT_CONFIG_PATH):
    with open(path) as f:
        return yaml.safe_load(f)


def main():
    parser = argparse.ArgumentParser(description="Nightly backup (#53).")
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--dry-run", action="store_true",
                        help="Print what would be backed up; write and upload nothing.")
    args = parser.parse_args()
    config = load_config(args.config)
    run_backup(config, config_path=args.config, dry_run=args.dry_run)


if __name__ == "__main__":
    main()
