"""Tests for the health checker (issue #54)."""

from datetime import datetime, timedelta, timezone

from healthcheck import (
    find_silent_nodes, check_health, decide_alert,
    acknowledge_episode, fresh_state,
)

NOW = datetime(2026, 11, 25, 12, 0, tzinfo=timezone.utc)


def test_no_silent_nodes():
    last_seen = {
        "indoor1": NOW - timedelta(minutes=5),
        "outdoor1": NOW - timedelta(minutes=10),
    }
    assert find_silent_nodes(last_seen, NOW, 30) == []


def test_one_silent_node():
    last_seen = {
        "indoor1": NOW - timedelta(minutes=5),
        "outdoor2": NOW - timedelta(minutes=45),
    }
    assert find_silent_nodes(last_seen, NOW, 30) == [("outdoor2", 45.0)]


def test_silent_sorted_worst_first():
    last_seen = {
        "indoor1": NOW - timedelta(minutes=40),
        "outdoor1": NOW - timedelta(minutes=90),
        "outdoor2": NOW - timedelta(minutes=35),
    }
    result = find_silent_nodes(last_seen, NOW, 30)
    assert [n for n, _ in result] == ["outdoor1", "indoor1", "outdoor2"]


def test_threshold_boundary():
    # Exactly at the threshold is not silent; just past it is.
    last_seen = {"indoor1": NOW - timedelta(minutes=30)}
    assert find_silent_nodes(last_seen, NOW, 30) == []
    last_seen = {"indoor1": NOW - timedelta(minutes=30, seconds=1)}
    assert len(find_silent_nodes(last_seen, NOW, 30)) == 1


def test_empty_input():
    assert find_silent_nodes({}, NOW, 30) == []


class FakeDB:
    def __init__(self, last_seen, hidden=frozenset()):
        self._last_seen = last_seen
        self._hidden = hidden

    def get_last_seen(self):
        return self._last_seen

    def get_hidden_nodes(self):
        return self._hidden


def test_check_health_excludes_hidden_nodes():
    db = FakeDB(
        {
            "indoor1": "2026-11-25T11:55:00+00:00",
            "indoor9": "2026-11-12T16:55:00+00:00",
        },
        hidden={"indoor9"},
    )
    status = check_health(db, 30, now=NOW)
    assert status["total_nodes"] == 1
    assert status["silent"] == []
    assert status["all_silent"] is False


def test_check_health_all_silent():
    db = FakeDB(
        {
            "indoor1": "2026-11-25T10:00:00+00:00",
            "outdoor1": "2026-11-25T10:05:00+00:00",
        }
    )
    status = check_health(db, 30, now=NOW)
    assert status["all_silent"] is True
    assert [n for n, _ in status["silent"]] == ["indoor1", "outdoor1"]

def _status(silent, all_silent=False):
    return {"checked_at": "2026-11-25T12:00:00+00:00", "total_nodes": 5,
            "silent": silent, "all_silent": all_silent}


def test_decide_new_episode_down():
    kind, title, message = decide_alert(
        fresh_state(), _status([("outdoor2", 45.0)]), NOW)
    assert kind == "down"
    assert "outdoor2" in message


def test_decide_new_episode_all_silent():
    kind, title, message = decide_alert(
        fresh_state(),
        _status([("indoor1", 40.0), ("outdoor1", 35.0)], all_silent=True),
        NOW,
    )
    assert kind == "down"
    assert title == "Home Climate: system down"


def test_decide_ongoing_not_due():
    state = fresh_state()
    state.update({"silent": True,
                  "silent_since": "2026-11-25T11:00:00+00:00",
                  "next_alert_at": "2026-11-25T13:00:00+00:00"})
    assert decide_alert(state, _status([("outdoor2", 60.0)]), NOW) == (
        None, None, None)


def test_decide_ongoing_due_realert():
    state = fresh_state()
    state.update({"silent": True,
                  "silent_since": "2026-11-25T11:00:00+00:00",
                  "next_alert_at": "2026-11-25T12:00:00+00:00"})
    kind, title, _ = decide_alert(state, _status([("outdoor2", 60.0)]), NOW)
    assert kind == "realert"
    assert "still" in title.lower()


def test_decide_acked_within_24h():
    state = fresh_state()
    state.update({"silent": True,
                  "acked_at": "2026-11-25T11:30:00+00:00",
                  "next_alert_at": "2026-11-26T11:30:00+00:00"})
    assert decide_alert(state, _status([("outdoor2", 90.0)]), NOW) == (
        None, None, None)


def test_decide_recovered():
    state = fresh_state()
    state.update({"silent": True,
                  "silent_since": "2026-11-25T11:00:00+00:00"})
    kind, title, _ = decide_alert(state, _status([]), NOW)
    assert kind == "recovered"


def test_decide_healthy_no_episode():
    assert decide_alert(fresh_state(), _status([]), NOW) == (
        None, None, None)


def test_acknowledge_sets_next_alert():
    state = fresh_state()
    state.update({"silent": True})
    assert acknowledge_episode(state, NOW, 24) is True
    assert state["acked_at"] == "2026-11-25T12:00:00+00:00"
    assert state["next_alert_at"] == "2026-11-26T12:00:00+00:00"


def test_acknowledge_no_episode():
    assert acknowledge_episode(fresh_state(), NOW, 24) is False
