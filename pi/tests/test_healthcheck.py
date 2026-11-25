"""Tests for the health checker (issue #54)."""

from datetime import datetime, timedelta, timezone

from healthcheck import find_silent_nodes, check_health

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
