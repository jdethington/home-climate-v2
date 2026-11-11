"""ntfy.sh alerts (issue #16): push notification on every OPEN/CLOSED flip.

No account, no extra dependency — plain urllib POST to https://ntfy.sh/<topic>.
Topics are public by default, so the topic name carries a random suffix.
The send never raises: if ntfy.sh is unreachable the service keeps running.
Quiet hours use the Pi's local time (Phoenix).
"""
import urllib.request
import urllib.error
from datetime import datetime


def is_quiet_hour(now, start, end):
    """True when `now` (hour 0-23) falls in the quiet window start..end.

    Handles the overnight wrap: 22..7 means 22:00-06:59.
    """
    hour = now.hour
    if start <= end:
        return start <= hour < end
    return hour >= start or hour < end

def send_notification(topic, title, message, quiet_start=22, quiet_end=7, respect_quiet_hours=True):
    """Send one ntfy push. Returns True if sent, False if skipped or failed."""
    if not topic:
        return False
    if respect_quiet_hours and is_quiet_hour(datetime.now(), quiet_start, quiet_end):
        print(f"ntfy: quiet hours, skipping '{title}'")
        return False
    req = urllib.request.Request(
        f"https://ntfy.sh/{topic}",
        data=message.encode("utf-8"),
        headers={"Title": title},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            print(f"ntfy: sent '{title}' (HTTP {resp.status})")
            return True
    except (urllib.error.URLError, OSError) as e:
        print(f"ntfy: failed to send '{title}': {e}")
        return False

def send_flip(topic, verdict, reasons, quiet_start=22, quiet_end=7):
    """Notify a recommendation flip (respects quiet hours)."""
    title = f"Windows {verdict}"
    message = reasons[0] if reasons else verdict
    return send_notification(topic, title, message, quiet_start, quiet_end)
