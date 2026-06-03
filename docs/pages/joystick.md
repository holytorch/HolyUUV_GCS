---
title: Joystick & control
layout: default
nav_order: 6
---

# Joystick & control
{: .no_toc }

Drive the vehicle with the on-screen sticks, arm it, and switch flight modes.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## The two sticks

Two circular pads float above the bottom bar. Drag a knob with the mouse; release and it springs
back to center. A small **±5% deadband** around the center reads as zero, and dragging past the
edge clamps to the rim.

| Pad | Axis | Effect |
|-----|------|--------|
| **Left — "Strafe / Fwd"** | vertical | Forward / reverse |
| | horizontal | Strafe left / right |
| **Right — "Yaw / Heave"** | vertical | Heave (ascend / descend) |
| | horizontal | Yaw left / right |

## What gets sent

While a link is connected, the station streams **MAVLink `MANUAL_CONTROL` at 50 Hz** — even when
the sticks are centered, so ArduSub keeps recognizing the ground station. The normalized stick
values map to the four axes:

| Field | From | Meaning |
|-------|------|---------|
| `x` | left stick, up = + | Forward (`-leftY × 1000`) |
| `y` | left stick, right = + | Strafe right (`leftX × 1000`) |
| `z` | right stick | Heave/throttle, `(1 − rightY) / 2 × 1000` — up = ascend, **center = 500** |
| `r` | right stick, right = + | Yaw right (`rightX × 1000`) |

`x`, `y`, `r` range **−1000…1000**; `z` ranges **0…1000** (500 = neutral).

{: .note }
> The sticks are a UI toggle and only transmit while a link is connected.

## Arming

Use the **ARM / DISARM** button in the Control Center. The top **ARM card** mirrors the live state
reported by the vehicle's HEARTBEAT.

{: .warning }
> An armed vehicle will spin its thrusters. Make sure it's safe before arming.

## Flight modes

The Control Center exposes three ArduSub modes as buttons; the active one (from HEARTBEAT) is
highlighted:

| Button | ArduSub mode |
|--------|--------------|
| **MANUAL** | direct manual control |
| **STABILIZE** | self-levelling |
| **ALT HOLD** | depth / altitude hold |

Under the hood the station can also address other ArduSub modes (ACRO, AUTO, GUIDED, CIRCLE,
SURFACE, POSHOLD, MOTOR_DETECT); only the three above are wired to buttons.

---

Next: [Telemetry]({% link pages/telemetry.md %}) — the MAVLink data behind these readouts.
