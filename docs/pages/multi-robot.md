---
title: Multiple Robots
layout: default
parent: Gazebo Simulation Guide
nav_order: 3
---

# Multiple Robots
{: .no_toc }

How to set up a simulation environment so several AUVs can be flown from one GCS. These are
configuration principles — they hold whether or not you use Project DAVE.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

![Multiple robots in the GCS, told apart by sysid]({{ '/assets/images/6.png' | relative_url }})

## How the GCS separates robots

The GCS binds **one** UDP port and receives MAVLink from every robot at the same time. It tells them
apart purely by the MAVLink **system ID (`sysid`)** carried in each packet — **not** by address. That
single fact decides how you configure the environment:

| What | How to set it | Why |
|---|---|---|
| **`sysid`** | **different** for every robot (1, 2, 3, …) | the only thing the GCS uses to distinguish them — must be unique |
| **IP** | **same** (`127.0.0.1` in sim) | one machine = one address; not used to tell robots apart |
| **GCS port** | **same** (`14550`) | every robot streams to the one endpoint the GCS listens on |
| **MAVROS** | **separate** — one per robot, own namespace | each robot needs its own bridge so links/topics don't collide |

> **In short:** same IP, same GCS port, one shared endpoint — separate the robots by **`sysid`**, and
> give each robot its **own MAVROS**.

---

## What to configure per robot

### A distinct `sysid` — the must

Set each robot's autopilot to a unique system ID. With ArduSub, pass it on the command line —
`--sysid 1`, `--sysid 2`, … — rather than relying on `SYSID_THISMAV` in a params file. Loaded via
`--defaults`, that parameter does **not** take effect on first boot, so both robots come up as
`sysid 1` and the GCS shows only one.

### One MAVROS per robot, in its own namespace

Run a separate MAVROS for each robot under a distinct namespace (`/robot1`, `/robot2`, …). Its params
file keys must include that namespace:

```yaml
/robot2/mavros_node:        # not a bare "mavros_node:" — that only matches the root namespace
  ros__parameters:
    fcu_url: "tcp://127.0.0.1:5770"
    gcs_url: "udp://0.0.0.0:14556@localhost:14550"   # distinct local port — see Joystick & commands
```

A bare `mavros_node:` key matches only a node at the root namespace, so a namespaced robot would
silently ignore its `fcu_url` / `gcs_url`.

### The same GCS endpoint for everyone

Every MAVROS forwards to the **same** address — `gcs_url: udp://@<gcs-ip>:14550`. The GCS binds
`14550` once and demultiplexes by `sysid`. The robots share the endpoint; only their `sysid` differs.

### (Simulation only) keep each instance on its own local ports

Two ArduSub SITL instances on one host can't share ports, so give each its own instance
(ArduSub `-I0`, `-I1`, …) — that offsets its internal FCU and physics ports automatically. Make sure
the Gazebo ArduPilotPlugin's `fdm_port_in` matches that instance's physics port, or the robot gets no
physics and just prints `No JSON sensor message received`.

This is internal plumbing between the simulator and the autopilot — from the GCS's side it's still
**one IP, one port, separated by `sysid`**.

---

## Port map (one host)

Everything that must be **unique per robot** when they share a machine. Only the GCS **remote** port
(`14550`) is shared — every other port is per-instance.

| Per robot | robot 1 | robot 2 | rule for robot N |
|---|---|---|---|
| ArduSub instance | `-I0` | `-I1` | `-I(N-1)` |
| **`sysid`** | `1` | `2` | `N` |
| MAVROS namespace | (root) | `/bluerov2_2` | `/robotN` |
| FCU link — MAVROS ↔ FC | `tcp 5760` | `tcp 5770` | `5760 + 10·(N-1)` |
| Physics — SDF `fdm_port_in` | `9002` | `9012` | `9002 + 10·(N-1)` |
| GCS link — **remote** (telemetry → GCS) | `udp 14550` | `udp 14550` | **same `14550`** |
| GCS link — **local bind** (commands ← GCS) | `14555` | `14556` | `14555 + (N-1)` |

`-I0/-I1/…` offsets the FCU and physics ports automatically (the `+10` rows). The one you set by hand
is the **last row** — the MAVROS local GCS-link port — because MAVROS defaults every robot to `14555`
and they'd collide. See the next section.

---

## Joystick & commands — give each MAVROS its own GCS-link port

Telemetry and commands are **not** symmetric. Every MAVROS sends telemetry *to* the same GCS address
(`14550`), but the GCS sends commands and the **joystick stream** *back* to wherever each robot's
packets came **from** — the MAVROS **local** GCS-link port.

MAVROS defaults that local port to **`14555`**. If two robots' MAVROS both bind `14555`, the GCS's
outgoing datagrams reach only **one** of them. The result is the classic multi-robot symptom:

{: .warning }
> A robot shows up with full telemetry, but **won't arm and won't respond to the joystick.** Its
> commands are landing on the *other* robot's MAVROS.

`target_system` can't save you here: the OS routes the UDP datagram by **port**, *before* any MAVLink
is parsed — so the packet lands on the wrong MAVROS and is gone. The fix is to give each robot's
GCS-link a **distinct local bind port**:

```yaml
# robot1
gcs_url: "udp://@localhost:14550"               # binds local 14555 (mavros default)
# robot2
gcs_url: "udp://0.0.0.0:14556@localhost:14550"  # bind a distinct local port → commands reach robot2
```

Now the GCS sees two separate return addresses (`…:14555`, `…:14556`) and delivers each robot's
joystick and commands to the right MAVROS.

---

## Simulation vs real hardware

The addressing changes, but the rule that matters never does — **every robot needs a distinct `sysid`**.

| | Simulation (one machine) | Real hardware |
|---|---|---|
| IP | same (`127.0.0.1`) | **different** per robot |
| GCS port | same (`14550`) | same (`14550`) |
| `sysid` | **different** | **different** |

On real robots each vehicle is its own machine with its own IP; in simulation they all live on one
host, so the GCS leans entirely on `sysid` to keep them apart.
