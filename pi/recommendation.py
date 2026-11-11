"""Recommendation logic (issue #10): dew-point gate + hysteresis.

All thresholds live in config.yaml,
not here — tuning must never require a reflash.

Averages: v1 kept a 5-minute rolling average over 30-second readings
(10 samples). v2 nodes send every 5 minutes, so a literal 5-minute
window would hold a single reading. Instead the average covers the
last N readings (config: average_readings, default 3 = 15 minutes).
"""

import math
from collections import deque

# Magnus formula for dew point (Celsius)
MAGNUS_A = 17.271
MAGNUS_B = 237.7

def dew_point(temp_c, humidity):
    """Dew point in Celsius from temperature (C) and relative humidity (%)."""
    if temp_c is None or humidity is None or humidity <= 0:
        return None
    alpha = (MAGNUS_A * temp_c) / (MAGNUS_B + temp_c) + math.log(humidity / 100.0)
    return (MAGNUS_B * alpha) / (MAGNUS_A - alpha)

class NodeAverage:
    """Rolling average for one node, over the last N readings"""

    def __init__(self, window):
        self.readings = deque(maxlen=window)

    def add(self, temp_c, humidity):
        # Skip dead-sensor readings (NaN) so one bad reading doesn't ruin the average
        # the node is still alive, just blind
        if temp_c is None or humidity is None:
            return
        if math.isnan(temp_c) or math.isnan(humidity):
            return
        self.readings.append((temp_c, humidity))

    def average(self):
        if not self.readings:
            return None
        n = len(self.readings)
        temp = sum(r[0] for r in self.readings) / n
        hum = sum(r[1] for r in self.readings) / n
        return temp, hum, dew_point(temp, hum)

class Recommender:
    """OPEN/CLOSED verdict from indoor vs outdoor averages.

    State latches: OPEN locks in at >= open_margin cooler and holds
    until the margin drops below hold_margin. The dew-point gate is
    evaluated every cycle: outdoor dew point must stay at least
    dewpoint_margin below indoor dew point, or the verdict closes.
    """

    def __init__(self, config=None):
        cfg = (config or {}).get("recommendation", {})
        self.open_margin = cfg.get("open_margin_c", 2.0)
        self.hold_margin = cfg.get("hold_margin_c", 1.0)
        self.dewpoint_margin = cfg.get("dewpoint_margin_c", 1.0)
        self.window = cfg.get("average_readings", 3)
        self.indoor = {}
        self.outdoor = {}
        self.state = "CLOSED"  # initial state

    def _group(self, node_id):
        # Node IDs carry their zone: indoor1, outdoor1, etc.
        if node_id.startswith("indoor"):
            return self.indoor
        elif node_id.startswith("outdoor"):
            return self.outdoor
        else:
            return None

    def update(self, node_id, temp_c, humidity):
        group = self._group(node_id)
        if group is None:
            return
        avg = group.get(node_id)
        if avg is None:
            avg = NodeAverage(self.window)
            group[node_id] = avg
        avg.add(temp_c, humidity)

    def _zone_average(self, group):
        """Average over all nodes in a zone (indoor or outdoor)."""
        temps, hums = [], []
        for avg in group.values():
            a = avg.average()
            if a is None:
                continue
            temps.append(a[0])
            hums.append(a[1])
        if not temps:
            return None
        temps = sum(temps) / len(temps)
        hums = sum(hums) / len(hums)
        return temps, hums, dew_point(temps, hums)

def recommend(self):
    """Return (state, reasons): the verdict and why, in plain words."""
    indoor = self._zone_average(self.indoor)
    outdoor = self._zone_average(self.outdoor)
    if indoor is None:
        return self.state, ["waiting for indoor readings"]
    if outdoor is None:
        return self.state, ["waiting for outdoor readings"]
    indoor_temp, indoor_hum, indoor_dp = indoor
    outdoor_temp, outdoor_hum, outdoor_dp = outdoor
    margin = indoor_temp - outdoor_temp
    reasons = []
    if margin >= 0:
        reasons.append(
            f"outdoor {outdoor_temp:.1f}C is {margin:.1f}C cooler "
            f"than indoor {indoor_temp:.1f}C"
        )
    else:
        reasons.append(
            f"outdoor {outdoor_temp:.1f}C is {-margin:.1f}C warmer "
            f"than indoor {indoor_temp:.1f}C"
        )
    # A missing dew point fails the gate closed — never open on
    # conditions you cannot verify.
    dp_ok = (indoor_dp is not None and outdoor_dp is not None
             and outdoor_dp <= indoor_dp - self.dewpoint_margin)
    if indoor_dp is None or outdoor_dp is None:
        reasons.append("dew point unavailable (missing humidity)")
    elif dp_ok:
        reasons.append(
            f"outdoor dew point {outdoor_dp:.1f}C is below "
            f"indoor {indoor_dp:.1f}C"
        )
    else:
        reasons.append(
            f"outdoor dew point {outdoor_dp:.1f}C is above "
            f"indoor {indoor_dp:.1f}C "
            f"(needs <= {indoor_dp - self.dewpoint_margin:.1f}C)"
        )
    if self.state == "OPEN":
        if margin < self.hold_margin or not dp_ok:
            self.state = "CLOSED"
            if margin < self.hold_margin:
                reasons.append(
                    f"closing: margin {margin:.1f}C below "
                    f"hold {self.hold_margin:.1f}C"
                )
            else:
                reasons.append("closing: dew point gate failed")
        else:
            reasons.append(
                f"staying open: margin {margin:.1f}C above "
                f"hold {self.hold_margin:.1f}C"
            )
    else:  # CLOSED
        if margin >= self.open_margin and dp_ok:
            self.state = "OPEN"
            reasons.append(
                f"opening: margin {margin:.1f}C meets "
                f"open {self.open_margin:.1f}C"
            )
        elif margin < self.open_margin:
            reasons.append(
                f"staying closed: margin {margin:.1f}C below "
                f"open {self.open_margin:.1f}C"
            )
    return self.state, reasons
