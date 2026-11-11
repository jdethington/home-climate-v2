"""Quick checks for notifier.py (issue #16). Run from pi/:
source venv/bin/activate
python test_notifier.py
"""
from datetime import datetime
from notifier import is_quiet_hour

# Overnight window 22..7
assert is_quiet_hour(datetime(2026, 11, 11, 23, 0), 22, 7) is True
assert is_quiet_hour(datetime(2026, 11, 11, 3, 0), 22, 7) is True
assert is_quiet_hour(datetime(2026, 11, 11, 12, 0), 22, 7) is False
assert is_quiet_hour(datetime(2026, 11, 11, 7, 0), 22, 7) is False
assert is_quiet_hour(datetime(2026, 11, 11, 22, 0), 22, 7) is True
# Same-day window 1..5
assert is_quiet_hour(datetime(2026, 11, 11, 3, 0), 1, 5) is True
assert is_quiet_hour(datetime(2026, 11, 11, 12, 0), 1, 5) is False
print("OK")
