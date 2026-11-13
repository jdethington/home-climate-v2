"""Tests for pi/recommendation.py (issue #24)."""

import math

import pytest

from recommendation import Recommender, NodeAverage, dew_point


# --- dew point (Magnus formula) ---

def test_dew_point_known_value():
    # 25C at 50% RH -> dew point ~13.8C (standard Magnus result).
    assert dew_point(25, 50) == pytest.approx(13.8, abs=0.1)


def test_dew_point_at_saturation():
    # At 100% RH the dew point equals the air temperature.
    assert dew_point(20, 100) == pytest.approx(20.0, abs=0.01)


def test_dew_point_missing_data():
    assert dew_point(None, 50) is None
    assert dew_point(25, None) is None
    assert dew_point(25, 0) is None


# --- rolling average ---

def test_average_of_three():
    avg = NodeAverage(3)
    avg.add(20, 40)
    avg.add(22, 40)
    avg.add(24, 40)
    temp, hum, dp = avg.average()
    assert temp == pytest.approx(22.0)
    assert hum == pytest.approx(40.0)


def test_average_drops_oldest():
    avg = NodeAverage(3)
    for t in (20, 22, 24, 26):
        avg.add(t, 40)
    temp, _, _ = avg.average()
    assert temp == pytest.approx(24.0)  # (22 + 24 + 26) / 3


def test_average_skips_nan():
    avg = NodeAverage(3)
    avg.add(20, 40)
    avg.add(math.nan, 40)
    avg.add(22, 40)
    temp, _, _ = avg.average()
    assert temp == pytest.approx(21.0)


def test_average_empty_is_none():
    assert NodeAverage(3).average() is None


# --- recommender (hysteresis + dew-point gate) ---
# window=1 so each update fully replaces the average — the window
# behavior is covered by the NodeAverage tests above.

def _recommender():
    return Recommender({"recommendation": {"average_readings": 1}})


def test_opens_when_cooler_and_dry():
    r = _recommender()
    r.update("indoor1", 25, 40)
    r.update("outdoor1", 20, 30)
    state, _ = r.recommend()
    assert state == "OPEN"


def test_stays_closed_below_open_margin():
    r = _recommender()
    r.update("indoor1", 25, 40)
    r.update("outdoor1", 24, 30)  # margin 1.0C < open 2.0C
    state, _ = r.recommend()
    assert state == "CLOSED"


def test_hysteresis_holds_then_closes():
    r = _recommender()
    r.update("indoor1", 25, 40)
    r.update("outdoor1", 20, 30)
    assert r.recommend()[0] == "OPEN"
    # Margin 1.0C: at the hold threshold, still open.
    r.update("outdoor1", 24, 30)
    assert r.recommend()[0] == "OPEN"
    # Margin 0.5C: below hold, closes.
    r.update("outdoor1", 24.5, 30)
    assert r.recommend()[0] == "CLOSED"


def test_dew_point_gate_blocks_opening():
    r = _recommender()
    r.update("indoor1", 25, 40)
    # 5C cooler, but humid outside — outdoor dew point is above indoor.
    r.update("outdoor1", 20, 90)
    state, _ = r.recommend()
    assert state == "CLOSED"


def test_dew_point_gate_closes_open_verdict():
    r = _recommender()
    r.update("indoor1", 25, 40)
    r.update("outdoor1", 20, 30)
    assert r.recommend()[0] == "OPEN"
    r.update("outdoor1", 20, 90)  # humidity jumps, gate fails
    assert r.recommend()[0] == "CLOSED"


def test_waiting_for_readings():
    r = _recommender()
    state, reasons = r.recommend()
    assert state == "CLOSED"
    assert "waiting for indoor readings" in reasons
    r.update("indoor1", 25, 40)
    _, reasons = r.recommend()
    assert "waiting for outdoor readings" in reasons


def test_unknown_node_ids_ignored():
    r = _recommender()
    r.update("node-B204", 20, 30)  # provisional id, no zone
    _, reasons = r.recommend()
    assert "waiting for indoor readings" in reasons
