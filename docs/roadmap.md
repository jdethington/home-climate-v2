# Roadmap

Where the project has been and where it's going. Issue numbers refer to
the tracker; the phases below are the planned build order.

## Done

- Phase 1 — Pi foundation (#1–#4)
- Phase 2 — Sensor net (#5–#9)
- Phase 3 — Pi brain (#10–#14)
- Phase 4 — Remote + alerts (#16, #17; #15 deferred)
- Phase 5 — Hardening (#18–#24; #25 deferred)
- Gateway LEDs + sensor (PR #50)
- LED node tracking (PR #51)

Phase 6 (stretch sensors, #26–#30) was deferred behind the foundation
work below — its issues land in Phase 13.

## Phase 7 — Protect the foundation

Small, Pi-side, no dependencies. Data safety and failure visibility first.

- #53 Pi backup — nightly copy of the database off the Pi.
- #54 Health monitoring — ntfy alert if no readings arrive for ~30 min.
- #64 Config validation — fail fast with a clear error on a bad config.yaml.

## Phase 8 — Data model

- #63 Node identity redesign — stable device IDs (MAC suffix), locations,
  zones; history survives replacement. The largest item on the list and
  the foundation for Phases 9–10 and #30.

## Phase 9 — Data quality + retention

- #52 Sensor data quality — filter garbage at ingestion, flag suspect
  sensors, purge test nodes + bad readings already in the DB.
- #62 Data retention — raw 2 years, hourly forever, daily high/low/avg.
  Aggregates per location, so after #63.

## Phase 10 — Dashboard

- #58 Dashboard auth — a password before the settings page ships.
- #61 Data export — download history as CSV.
- #60 Dashboard overhaul — usability redesign + settings page. After #63
  (cards per location) and #58 (settings need protection).

## Phase 11 — Gateway management

- #56 Nodes learn from the gateway beacon — channel + gateway MAC.
- #57 Remote gateway updates — Pi-triggered gateway OTA.
- #55 Gateway LED polish — marker cleanup + RGB semantics.

## Phase 12 — Permanent hardware

- #25 Permanent enclosures + solar + battery + mounting. Needs parts +
  the custom PCB.
- #59 Low-battery alert — ntfy when a node's battery is low. Needs real
  battery readings.

## Phase 13 — Sensors + access + product

- #27 BME280 upgrade path (enables #28).
- #28 Pressure-trend dust detector.
- #29 SCD40 CO2 sensing.
- #30 TVOC differential + per-zone recommendations. Needs two BME688
  nodes + the zones from #63.
- #15 Permanent URL + fridge QR. Blocked only on the domain cost decision.
- Wind node (no issue yet) — wireless roof/eave wind sensor. After #25.
- #26 Pre-flashed Pi SD image + product items (per-device AP password,
  cloud backend). Deferred.

## Decision points

- Parts purchases gate #25, #27, #29, #30.
- The domain cost gates #15.
- Everything else is build time.
