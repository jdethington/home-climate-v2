"""Quick checks for recommendation.py (issue #10). Run from pi/:
source venv/bin/activate
python test_recommendation.py
"""
import yaml
from recommendation import Recommender, dew_point

# 1. Real readings from the live nodes — temp says open, dew point says no
r = Recommender()
r.update("indoor1", 27.3, 34)
r.update("outdoor1", 25.3, 57.1)
state, reasons = r.recommend()
print("real readings:", state, reasons)   # expect CLOSED
assert state == "CLOSED"
assert any("dew point" in reason for reason in reasons)

# 2. Cool dry evening — both gates pass
r = Recommender()
r.update("indoor1", 27, 40)
r.update("outdoor1", 22, 40)
state, reasons = r.recommend()
print("cool dry evening:", state, reasons)   # expect OPEN
assert state == "OPEN"
assert any("cooler" in reason for reason in reasons)

# 3. Hysteresis — window of 1 so each update replaces the last
r = Recommender({"recommendation": {"average_readings": 1}})
r.update("indoor1", 27, 40)
r.update("outdoor1", 22, 40)
print("open:", r.recommend())            # expect OPEN
r.update("indoor1", 26, 40)
r.update("outdoor1", 24.5, 40)
print("margin 1.5:", r.recommend())     # expect OPEN (holds above 1.0)
r.update("indoor1", 25, 40)
r.update("outdoor1", 24.5, 40)
print("margin 0.5:", r.recommend())     # expect CLOSED

# 4. Config file loads and the thresholds come from it
with open("config.yaml") as f:
    cfg = yaml.safe_load(f)
r = Recommender(cfg)
print("config margins:", r.open_margin, r.hold_margin, r.dewpoint_margin)
print("dew points: indoor %.1fC, outdoor %.1fC" % (
    dew_point(27.3, 34), dew_point(25.3, 57.1)))
