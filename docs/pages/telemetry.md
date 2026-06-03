---
title: Telemetry
layout: default
nav_order: 7
---

# Telemetry (MAVLink data)
{: .no_toc }

Which MAVLink messages the station decodes, and where each value appears on screen.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Incoming messages

The station parses these MAVLink messages from the active vehicle:

| Message | Fields used | Rendered as |
|---------|-------------|-------------|
| **HEARTBEAT** | arm state, custom (flight) mode, system ID | ARM card, mode buttons, `sys_id` card, link watchdog |
| **ATTITUDE** | roll, pitch, yaw (rad) | Attitude indicator |
| **SYS_STATUS** | battery %, voltage, current, comm drop rate / errors | Battery readout & vehicle cards |
| **RADIO_STATUS** | RSSI, remote RSSI | Vehicle-card signal bars |
| **VFR_HUD** | groundspeed, altitude, heading, throttle | Speed, depth gauge, compass, throttle ring |
| **GLOBAL_POSITION_INT** | latitude, longitude | Lat/Lon card, map marker |
| **GPS_RAW_INT** | satellite count, HDOP | GPS status popup |
| **COMMAND_ACK** | command result | Log feed |

## A few conventions

- **Depth** comes from `VFR_HUD.altitude`, which is **negative underwater** — the depth gauge
  shows its magnitude as `-X.X m`.
- **Attitude** is stored in **radians** and converted to degrees for display.
- **Groundspeed** is in **m/s** and shown in **km/h** (×3.6).
- **Throttle** can be negative (reverse) on ArduSub; the ring shows its **magnitude**.
- **Battery `%` = −1** means "not reported" and renders as `—`.

## Outgoing messages

The station sends:

| Message | When | Purpose |
|---------|------|---------|
| **HEARTBEAT** | 1 Hz while connected | Announce the GCS to the vehicle |
| **MANUAL_CONTROL** | 50 Hz while connected | Joystick axes (see [Joystick & control]({% link pages/joystick.md %})) |
| **COMMAND_LONG** (`MAV_CMD_COMPONENT_ARM_DISARM`) | on ARM / DISARM | Arm or disarm |
| **SET_MODE** (custom mode) | on a mode button | Switch flight mode |

---

That's the full data path — from MAVLink on the wire to the dark-themed readouts on screen.
