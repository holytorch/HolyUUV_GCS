---
title: Interface
layout: default
nav_order: 5
---

# Interface guide
{: .no_toc }

What every card, gauge, and control on the main screen does.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

![HolyUUV GCS interface]({{ '/assets/images/example.png' | relative_url }})

## Top bar

From left to right:

| Element | What it shows / does |
|---------|----------------------|
| **Logo** | HolyUUV GCS. |
| **`sys_id` card** | Active vehicle's system ID. Click to open the **connections + vehicles** popup (see [Connecting]({% link pages/connecting.md %})). |
| **Link card** | `● LINKED` (green) while the vehicle HEARTBEAT is alive, `● NO LINKED` (red) otherwise. |
| **ARM card** | `ARMED` / `DISARMED` — the vehicle's current arm state. |
| **Compass ribbon** | Heading tape (top center): cardinal (N/E/S/W) + heading in degrees, scrolling with the vehicle's heading. |
| **Lat/Lon card** | Live GPS coordinates of the active vehicle (`° N/S, ° E/W`). |
| **Satellite button** | Click to open **GPS status**: fix type, satellite count, and HDOP. |

## Map controls (right edge)

| Control | What it does |
|---------|--------------|
| **Map mode button** | Opens the **MODE** popup: **Dark** (OSM), **Bright** (Voyager), or **3D** terrain. |
| **Zoom + / −** | Zoom the 2D map in/out; the middle readout shows the current zoom level. Hidden in 3D mode. |

### Map modes

| Mode | Description |
|------|-------------|
| **Dark** | Dark-themed street map (default). |
| **Bright** | Light "Voyager" street map. |
| **3D** | Qt3D terrain view with an orbit camera — drag to look around, scroll to move. |

The active vehicle is drawn as a **cyan dot**, and the map auto-centers on the first GPS fix.

## Flight instruments (right side)

| Gauge | Source | What it shows |
|-------|--------|---------------|
| **Depth gauge** | `VFR_HUD` altitude | Current depth on a 0–100 m scale, with a `-X.X m` readout. |
| **Attitude indicator** | `ATTITUDE` | Artificial horizon — roll & pitch, with a pitch ladder and roll arc. |
| **Compass ribbon** | `VFR_HUD` heading | Heading (shown in the top bar). |

## Bottom bar

### LOG (left half)

A scrolling event/console log. Toggle it with the **LOG** button — when collapsed it tucks to the
bottom edge to free up the map.

### Control Center (right half)

Empty until you pick a vehicle (`Select a vehicle from VEHICLES`). Once one is active it shows:

| Block | Source | What it shows |
|-------|--------|---------------|
| **UUV illustration** | — | Static vehicle image. |
| **Battery** | `SYS_STATUS` | Remaining `%` and pack voltage `V`; the bar turns amber below 50%, red below 20%. |
| **Speed** | `VFR_HUD` groundspeed | Ground speed in km/h. |
| **Throttle** | `VFR_HUD` throttle | Throttle magnitude as a ring + `%`. |
| **ARM / DISARM** | command | Toggles the vehicle's arm state. |
| **Mode buttons** | command | `MANUAL`, `STABILIZE`, `ALT HOLD` — the active mode is highlighted. |

{: .note }
> Arming, flight modes, and the joysticks are covered in [Joystick & control]({% link pages/joystick.md %}).

## Joysticks

Two circular pads float above the bottom bar — **Strafe / Fwd** (left) and **Yaw / Heave**
(right). See [Joystick & control]({% link pages/joystick.md %}).

---

Next: [Joystick & control]({% link pages/joystick.md %}).
