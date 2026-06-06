---
title: Telemetry
layout: default
nav_order: 7
---

# Telemetry — MAVLink Data Reference
{: .no_toc }

A complete reference of every MAVLink message the station decodes, the raw wire format,
unit conversions applied, and where each value is rendered.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Architecture

```
Vehicle / Simulator
        │  UDP / TCP  (MAVLink v2 framing)
        ▼
  LinkManager          — byte stream ingestion
        │
  MavlinkManager       — packet parser (mavlink_parse_char)
        │  typed signals per message ID
        ▼
  VehicleState         — normalised telemetry store (QObject properties)
        │  Qt property-change signals
        ▼
  QML UI               — live bindings to every gauge and card
```

The parser runs `mavlink_parse_char()` on every incoming byte.
When a complete frame is verified, it dispatches on `msgid` to a typed handler that emits a
signal carrying a plain-C struct. `VehicleState` receives these signals and updates its
properties, which drive QML bindings at the next render cycle.

---

## Incoming messages

### HEARTBEAT (ID 0)

| Wire field | Type | Used as |
|---|---|---|
| `base_mode` | `uint8_t` | bit 7 (`MAV_MODE_FLAG_SAFETY_ARMED`) → `armed` |
| `custom_mode` | `uint32_t` | mapped to flight-mode string (see table below) |
| `system_id` *(frame header)* | `uint8_t` | vehicle discovery / `sysid` card |

**Flight-mode mapping (ArduSub custom_mode):**

| `custom_mode` | Mode string |
|---|---|
| 0 | `STABILIZE` |
| 2 | `ALT_HOLD` |
| 4 | `GUIDED` |
| 16 | `POSHOLD` |
| 19 | `MANUAL` |
| other | raw integer string |

**Rendered as:** ARM card, mode buttons (active highlight), `sys_id` card, link watchdog.

A GCS-side watchdog timer is reset on every received HEARTBEAT.
If no HEARTBEAT arrives for **5 seconds**, `heartbeatOk` flips to `false` and the link is
considered lost.

---

### ATTITUDE (ID 30)

| Wire field | Type | Unit | Stored as |
|---|---|---|---|
| `roll` | `float` | rad | `VehicleState::roll` |
| `pitch` | `float` | rad | `VehicleState::pitch` |
| `yaw` | `float` | rad | `VehicleState::yaw` |

All three values are kept in **radians** internally. The attitude indicator converts them to
degrees at bind time:

```
rollDeg  = vehicle.roll  × (180 / π)
pitchDeg = vehicle.pitch × (180 / π)
```

`yaw` is not currently displayed separately (heading comes from `VFR_HUD` instead).

**Rendered as:** Attitude indicator (artificial horizon, roll arc, pitch ladder).

---

### SYS_STATUS (ID 1)

| Wire field | Type | Raw unit | Conversion | Stored as |
|---|---|---|---|---|
| `voltage_battery` | `uint16_t` | mV | ÷ 1000 | `voltage` (V) |
| `current_battery` | `int16_t` | cA (×10 mA) | ÷ 100 | `current` (A) |
| `battery_remaining` | `int8_t` | % | — | `batteryRemaining` |
| `drop_rate_comm` | `uint16_t` | 0–10 000 (×0.01 %) | — | `dropRateComm` |
| `errors_comm` | `uint16_t` | count | — | `errorsComm` |

`battery_remaining = -1` means the vehicle does not report this value; the UI renders `—`.

Battery bar colour thresholds:

| Remaining | Colour |
|---|---|
| ≥ 50 % | Green (`#aaff00`) |
| 20 – 49 % | Amber (`#ffb347`) |
| < 20 % | Red (`#ff5252`) |

**Rendered as:** Battery bar + `%  V` readout in Control Center; vehicle-card battery indicator.

---

### RADIO_STATUS (ID 109)

| Wire field | Type | Description |
|---|---|---|
| `rssi` | `uint8_t` | Local receive signal strength (0–254; 255 = unknown) |
| `remrssi` | `uint8_t` | Remote (vehicle-side) signal strength |
| `noise` | `uint8_t` | Local noise floor |
| `remnoise` | `uint8_t` | Remote noise floor |
| `rxerrors` | `uint16_t` | Receive error count |

Only `rssi` is currently used to drive the signal-bar level in vehicle cards
(0 bars = no signal, 3 bars = full).

**Rendered as:** Signal-strength bars on each vehicle card in the VEHICLES popup.

---

### VFR_HUD (ID 74)

| Wire field | Type | Raw unit | Conversion | Stored as |
|---|---|---|---|---|
| `groundspeed` | `float` | m/s | × 3.6 for display | `groundspeed` (m/s) |
| `altitude` | `float` | m (negative = below surface) | `depth = max(0, −altitude)` | `depth` (m) |
| `heading` | `float` | degrees (0–360) | — | `heading` (°) |
| `throttle` | `int` | % (can be negative on ArduSub) | `abs(throttle)` for display | `throttle` (%) |

{: .note }
> ArduSub reports depth as a **negative altitude** (e.g. `-3.2 m` = 3.2 m below surface).
> The depth gauge takes `max(0, −altitude)` so it always shows a positive magnitude.

**Rendered as:**
- `groundspeed` → speed readout in km/h (`× 3.6`)
- `altitude` → depth gauge (`−X.X m`)
- `heading` → compass ribbon (N/E/S/W + degrees)
- `throttle` → throttle ring (magnitude, 0–100 %)

---

### GLOBAL_POSITION_INT (ID 33)

| Wire field | Type | Raw unit | Conversion | Stored as |
|---|---|---|---|---|
| `lat` | `int32_t` | degE7 (×10⁻⁷ °) | ÷ 10 000 000 | `latitude` (°) |
| `lon` | `int32_t` | degE7 (×10⁻⁷ °) | ÷ 10 000 000 | `longitude` (°) |
| `alt` | `int32_t` | mm | ÷ 1000 | altitude (m, unused) |
| `relative_alt` | `int32_t` | mm | ÷ 1000 | relative altitude (m, unused) |

On the first non-zero GPS fix from the active vehicle, the map smoothly pans to
`(latitude, longitude)` using an ease-out cubic animation (duration 1 400 ms, 60 fps).

**Rendered as:** Lat/Lon card (top-right), cyan dot marker on the map.

---

### GPS_RAW_INT (ID 24)

| Wire field | Type | Raw unit | Stored as |
|---|---|---|---|
| `satellites_visible` | `uint8_t` | count | `gpsSatCount` |
| `eph` | `uint16_t` | HDOP × 100 | `gpsHdop` (÷ 100) |

Fix type is inferred:

```
fix = (gpsSatCount >= 3 && (latitude != 0 || longitude != 0)) ? "3D" : "No fix"
```

**Rendered as:** GPS status popup (satellite icon button, top-right) — Fix type, satellite count, HDOP.

---

### COMMAND_ACK (ID 77)

Acknowledged command results (arm/disarm, mode change) are forwarded to the log feed.

---

## Outgoing messages

### HEARTBEAT (ID 0) — 1 Hz

The GCS announces itself to ArduSub at **1 Hz** so the vehicle keeps its GCS-present flag set.
Sent with `type = MAV_TYPE_GCS`, `autopilot = MAV_AUTOPILOT_INVALID`.

---

### MANUAL_CONTROL (ID 69) — 50 Hz

Joystick axes streamed at **50 Hz** (20 ms interval) while a link is connected.
Sent even when all axes are at neutral so ArduSub keeps the GCS-connected heartbeat alive.

| Field | Range | Source |
|---|---|---|
| `x` | −1000 … +1000 | `−leftY × 1000` (up = forward) |
| `y` | −1000 … +1000 | `leftX × 1000` (right = strafe right) |
| `z` | 0 … 1000 | `(1 − rightY) / 2 × 1000` (500 = neutral depth) |
| `r` | −1000 … +1000 | `rightX × 1000` (right = yaw right) |
| `buttons` | `uint16_t` bitmask | 0 (unused) |

---

### COMMAND_LONG (ID 76) — on ARM / DISARM

Encodes `MAV_CMD_COMPONENT_ARM_DISARM` (400).
`param1 = 1.0` to arm, `param1 = 0.0` to disarm.

---

### SET_MODE (ID 11) — on mode button

Sets `base_mode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED (0x01)` and
`custom_mode` to the ArduSub mode number (see HEARTBEAT table above).

---

## VehicleState property summary

All values exposed to QML via `Q_PROPERTY` with change signals:

| Property | Type | Signal |
|---|---|---|
| `sysid` | `int` | `sysidChanged` |
| `batteryRemaining` | `int` | `batteryChanged` |
| `voltage` | `float` | `batteryChanged` |
| `current` | `float` | `batteryChanged` |
| `roll`, `pitch`, `yaw` | `float` (rad) | `attitudeChanged` |
| `depth`, `groundspeed`, `heading` | `float` | `vfrHudChanged` |
| `throttle` | `int` | `vfrHudChanged` |
| `latitude`, `longitude` | `double` | `gpsChanged` |
| `gpsSatCount` | `int` | `gpsChanged` |
| `gpsHdop` | `float` | `gpsChanged` |
| `armed` | `bool` | `armedChanged` |
| `flightMode` | `QString` | `flightModeChanged` |
| `heartbeatOk` | `bool` | `heartbeatStatusChanged` |
| `dropRateComm`, `errorsComm` | `uint16_t` | `linkQualityChanged` |
| `rssi`, `remRssi` | `uint8_t` | `linkQualityChanged` |
