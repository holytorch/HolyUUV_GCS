---
title: Connecting
layout: default
nav_order: 4
---

# Connecting to a vehicle
{: .no_toc }

Link the station to a Gazebo simulator (or a real ArduSub vehicle) over MAVLink.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## How connections work

HolyUUV GCS speaks **MAVLink** (ArduSub / ArduPilot compatible) over **UDP** or **TCP**. It was
developed and validated against **Gazebo** simulation via
[**Project DAVE**](https://github.com/Field-Robotics-Lab/dave).

Once a link is open, the station:

- sends a **GCS HEARTBEAT at 1 Hz** so the vehicle knows a ground station is present,
- listens for vehicle **HEARTBEAT** messages to discover vehicles by system ID, and
- watches the active vehicle — if its HEARTBEAT goes silent for **5 seconds**, the link
  auto-disconnects.

{: .note }
> One active link at a time — connecting to a new endpoint closes the previous one.

## Step by step

1. **Start your simulator** (Gazebo / SITL) so it's streaming MAVLink — typically UDP `14555`.
2. In the GCS, click the **`sys_id`** card at the top-left to open the connections popup.
3. Under **CONNECTIONS**, click **＋ Add Connection**.
4. Choose **UDP**, set **Host** `127.0.0.1` and **Port** `14555` (the defaults), then **Add**.
   - For a remote vehicle, use its IP address and port instead.
5. Back in the popup, press **Connect** next to the entry.

The **link card** at the top switches from `● NO LINKED` (red) to `● LINKED` (green) as soon as
the vehicle's HEARTBEAT is received.

## Choosing the active vehicle

Every system ID that sends a HEARTBEAT appears as a card under **VEHICLES** in the same popup,
with its live battery and signal strength. Click a card to make it the **active vehicle**
(highlighted with a green border). Its telemetry then fills the Control Center and the gauges.

The `sys_id` card reflects the state:

| Display | Meaning |
|---------|---------|
| `sys_id : -` | not connected |
| `sys_id : detecting…` | connected, waiting for the first HEARTBEAT |
| `sys_id : N` | active vehicle is system ID *N* |

## Map auto-centering

On the **first GPS fix** from the active vehicle, the map smoothly pans to its position (once per
vehicle). The vehicle is drawn as a **cyan dot** on the map.

---

Next: [Interface]({% link pages/interface.md %}) — what every on-screen element means.
