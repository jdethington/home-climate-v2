"""NWS dust-storm alert polling (issue #17).

Polls the free api.weather.gov alerts endpoint for the Gilbert point and
returns only the active dust alerts (Dust Storm Warning, Blowing Dust
Advisory, ...). No API key; the API requires a User-Agent header.
Returns None on fetch failure (caller keeps the old data), [] when there
simply are no active dust alerts.
"""
import json
import urllib.request
import urllib.error

NWS_URL = "https://api.weather.gov/alerts/active?point={lat},{lon}"
USER_AGENT = "home-climate (https://github.com/jdethington/home-climate-v2)"


def filter_dust_alerts(features):
    """Pick the real dust alerts out of a raw NWS features list."""
    alerts = []
    for feature in features:
        props = feature.get("properties", {})
        if props.get("status") != "Actual":
            continue
        event = props.get("event", "")
        if "dust" not in event.lower():
            continue
        alerts.append({
            "id": props.get("id", ""),
            "event": event,
            "headline": props.get("headline", ""),
            "severity": props.get("severity", ""),
            "onset": props.get("onset", ""),
            "expires": props.get("expires", ""),
        })
    return alerts


def fetch_dust_alerts(latitude, longitude):
    """Return the list of currently active dust alerts for the point."""
    url = NWS_URL.format(lat=latitude, lon=longitude)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.load(resp)
    except (urllib.error.URLError, OSError, ValueError) as e:
        print(f"nws: fetch failed: {e}")
        return None
    return filter_dust_alerts(data.get("features", []))
