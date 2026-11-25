"""Health check (issue #54): is the system still reporting?

The checker asks the database when each node last reported and flags
any node silent longer than the configured threshold. Hidden nodes
(removed from the dashboard) are excluded — a removed node never
reports again, so flagging it would be a permanent false alarm.

This module only detects and reports. Alerting (ntfy, state, ack)
lives in module 2; scheduling in module 3.
"""

import argparse
import json
import os
from datetime import datetime, timedelta, timezone

import yaml

from database import Database
from notifier import send_notification

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

def fresh_state():
    return {
        "silent": False,
        "silent_since": None,
        "silent_nodes": [],
        "last_alert_at": None,
        "acked_at": None,
        "next_alert_at": None,
    }


def load_state(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return fresh_state()


def save_state(path, state):
    with open(path, "w") as f:
        json.dump(state, f, indent=2)


def format_alert(status, kind):
    """(title, message) for a down / realert / recovered alert."""
    if kind == "recovered":
        return ("Home Climate: recovered",
                "Readings resumed — all nodes reporting.")
    silent = status["silent"]
    if status["all_silent"]:
        longest = silent[0][1]
        title = ("Home Climate: still down" if kind == "realert"
                 else "Home Climate: system down")
        return (title,
                f"No readings from any node for {longest:.0f} min — "
                "gateway or broker may be down.")
    detail = ", ".join(
        f"{node_id} ({minutes:.0f} min)" for node_id, minutes in silent
    )
    title = ("Home Climate: still silent" if kind == "realert"
             else "Home Climate: node silent")
    return (title, f"Silent nodes: {detail}.")


def decide_alert(state, status, now):
    """What to send this run. Returns (kind, title, message) or Nones.

    kind is "down" (new episode), "realert" (still silent and due),
    or "recovered". The next-alert time lives in the state file, so
    this function needs no intervals — it just reads next_alert_at.
    """
    if status["silent"]:
        if not state.get("silent"):
            kind = "down"
        else:
            next_at = state.get("next_alert_at")
            if next_at is not None and now < parse_ts(next_at):
                return None, None, None
            kind = "realert"
        title, message = format_alert(status, kind)
        return kind, title, message
    if state.get("silent"):
        title, message = format_alert(status, "recovered")
        return "recovered", title, message
    return None, None, None


def acknowledge_episode(state, now, realert_after_ack_hours=24):
    """Acknowledge the current silent episode. Returns False if none."""
    if not state.get("silent"):
        return False
    state["acked_at"] = now.isoformat(timespec="seconds")
    state["next_alert_at"] = (
        now + timedelta(hours=realert_after_ack_hours)
    ).isoformat(timespec="seconds")
    return True


def send_health_alert(topic, title, message, ntfy_cfg):
    """Health alerts bypass quiet hours (issue #54 decision)."""
    return send_notification(
        topic, title, message,
        ntfy_cfg.get("quiet_start", 22),
        ntfy_cfg.get("quiet_end", 7),
        respect_quiet_hours=False,
    )


def print_status(status, threshold):
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


def run_check(config, threshold=None, dry_run=False, acknowledge=False):
    """One check cycle: detect, decide, alert, update state."""
    health_cfg = config.get("health", {})
    state_path = health_cfg.get("state_file", "health_state.json")
    state = load_state(state_path)
    now = datetime.now(timezone.utc)
    if acknowledge:
        if dry_run:
            print("[dry run] would acknowledge the current episode")
            return
        if acknowledge_episode(
            state, now, health_cfg.get("realert_after_ack_hours", 24)
        ):
            save_state(state_path, state)
            print(f"Acknowledged — next re-alert at {state['next_alert_at']}")
        else:
            print("Nothing to acknowledge — no silent episode.")
        return
    if threshold is None:
        threshold = health_cfg.get("silence_minutes", 30)
    db = Database(config.get("database", {}).get("path", "homeclimate.db"))
    status = check_health(db, threshold, now=now)
    print_status(status, threshold)
    kind, title, message = decide_alert(state, status, now)
    if kind is None:
        if status["silent"] and not dry_run:
            state["silent_nodes"] = [n for n, _ in status["silent"]]
            save_state(state_path, state)
        return
    if dry_run:
        print(f"[dry run] would send '{title}': {message}")
        return
    topic = os.environ.get("NTFY_TOPIC", "")
    sent = send_health_alert(topic, title, message, config.get("ntfy", {}))
    if not sent:
        print("Alert not sent (no topic set, or ntfy unreachable) — "
              "state unchanged.")
        return
    if kind in ("down", "realert"):
        state["silent"] = True
        state["silent_since"] = (
            state.get("silent_since") or now.isoformat(timespec="seconds")
        )
        state["silent_nodes"] = [n for n, _ in status["silent"]]
        state["last_alert_at"] = now.isoformat(timespec="seconds")
        interval = (
            health_cfg.get("realert_after_ack_hours", 24)
            if state.get("acked_at")
            else health_cfg.get("realert_hours", 1)
        )
        state["next_alert_at"] = (
            now + timedelta(hours=interval)
        ).isoformat(timespec="seconds")
        save_state(state_path, state)
    else:  # recovered
        save_state(state_path, fresh_state())
        print("Recovery alert sent — episode cleared.")

def load_config(path=DEFAULT_CONFIG_PATH):
    with open(path) as f:
        return yaml.safe_load(f)


def main():
    parser = argparse.ArgumentParser(description="Check node silence (#54).")
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--threshold-minutes", type=float, default=None,
                        help="Override config health.silence_minutes (testing).")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print the alert that would be sent; send nothing, write nothing.")
    parser.add_argument("--acknowledge", action="store_true",
                        help="Acknowledge the current silent episode (re-alerts switch to 24h).")
    args = parser.parse_args()
    config = load_config(args.config)
    run_check(config, threshold=args.threshold_minutes,
              dry_run=args.dry_run, acknowledge=args.acknowledge)


if __name__ == "__main__":
    main()
