"""Quick checks for nws_alerts.py (issue #17). Run from pi/:
source venv/bin/activate
python test_nws_alerts.py
"""
from nws_alerts import filter_dust_alerts, fetch_dust_alerts

# The filter, with hand-built NWS-shaped features.
fake_features = [
    {"properties": {"id": "1", "status": "Actual", "event": "Dust Storm Warning",
                    "headline": "Dust Storm Warning in effect", "severity": "Severe",
                    "onset": "2026-11-11T12:00:00-07:00", "expires": "2026-11-11T18:00:00-07:00"}},
    {"properties": {"id": "2", "status": "Actual", "event": "Heat Advisory",
                    "headline": "Heat Advisory", "severity": "Moderate",
                    "onset": "2026-11-11T12:00:00-07:00", "expires": "2026-11-11T18:00:00-07:00"}},
    {"properties": {"id": "3", "status": "Actual", "event": "Blowing Dust Advisory",
                    "headline": "Blowing Dust Advisory", "severity": "Moderate",
                    "onset": "2026-11-11T12:00:00-07:00", "expires": "2026-11-11T18:00:00-07:00"}},
    {"properties": {"id": "4", "status": "Test", "event": "Dust Storm Warning",
                    "headline": "Test warning", "severity": "Severe",
                    "onset": "2026-11-11T12:00:00-07:00", "expires": "2026-11-11T18:00:00-07:00"}},
]
dust = filter_dust_alerts(fake_features)
assert [a["id"] for a in dust] == ["1", "3"], dust
assert dust[0]["event"] == "Dust Storm Warning"

# Live smoke check against the real API (no key needed).
live = fetch_dust_alerts(33.3528, -111.7890)
assert live is not None, "live fetch failed"
print(f"Live NWS check OK: {len(live)} active dust alert(s) for Gilbert")
print("OK")
