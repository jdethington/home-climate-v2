"""Health check (issue #54): is the system still reporting?

The checker asks the database when each node last reported and flags
any node silent longer than the configured threshold. Hidden nodes
(removed from the dashboard) are excluded — a removed node never
reports again, so flagging it would be a permanent false alarm.

This module only detects and reports. Alerting (ntfy, state, ack)
lives in module 2; scheduling in module 3.
"""

import argparse
from datetime import datetime, timezone

import yaml

from database import Database

DEFAULT_CONFIG_PATH = "config.yaml"


def parse_ts(ts):
    """Parse a UTC ISO timestamp from the DB into an aware datetime."""
    dt = datetime.fromisoformat(ts)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt


def find_silent_nodes(last_seen, now, threshold_minutes):
    """Nodes silent longer than the threshold.

    last_seen: {node_id: datetime of last reading}.
    Returns [(node_id, silent_minutes), ...] sorted worst first.
    """
    silent = []
    for node_id, last in last_seen.items():
        minutes = (now - last).total_seconds() / 60
        if minutes > threshold_minutes:
            silent.append((node_id, minutes))
    silent.sort(key=lambda item: item[1], reverse=True)
    return silent


def check_health(db, threshold_minutes, now=None):
    """Check every known (non-hidden) node. Returns a status dict."""
    now = now or datetime.now(timezone.utc)
    last_seen = {
        node_id: parse_ts(ts) for node_id, ts in db.get_last_seen().items()
    }
    hidden = db.get_hidden_nodes()
    visible = {n: t for n, t in last_seen.items() if n not in hidden}
    silent = find_silent_nodes(visible, now, threshold_minutes)
    return {
        "checked_at": now.isoformat(timespec="seconds"),
        "total_nodes": len(visible),
        "silent": silent,
        "all_silent": bool(visible) and len(silent) == len(visible),
    }


def load_config(path=DEFAULT_CONFIG_PATH):
    with open(path) as f:
        return yaml.safe_load(f)


def main():
    parser = argparse.ArgumentParser(description="Check node silence (#54).")
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--threshold-minutes", type=float, default=None,
                        help="Override config health.silence_minutes (testing).")
    args = parser.parse_args()
    config = load_config(args.config)
    threshold = args.threshold_minutes
    if threshold is None:
        threshold = config.get("health", {}).get("silence_minutes", 30)
    db_path = config.get("database", {}).get("path", "homeclimate.db")
    db = Database(db_path)
    status = check_health(db, threshold)
    if status["total_nodes"] == 0:
        print("WARNING: no nodes in the database")
    elif not status["silent"]:
        print(f"OK: {status['total_nodes']} nodes reporting "
              f"(threshold {threshold} min)")
    else:
        for node_id, minutes in status["silent"]:
            print(f"SILENT: {node_id} — no reading for {minutes:.0f} min")
        if status["all_silent"]:
            print("SYSTEM DOWN: no readings from any node")


if __name__ == "__main__":
    main()
