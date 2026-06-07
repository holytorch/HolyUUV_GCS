---
title: GCS + Simulator
layout: default
nav_order: 4
---

# GCS + Simulator
{: .no_toc }

Connect HolyUUV GCS to a simulated underwater vehicle running in **Project DAVE**.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Overview

HolyUUV GCS was developed and validated using **Gazebo** simulation via
[**Project DAVE**](https://dave-ros2.notion.site/?v=d54cc8422868455888cc629d8e6117a9&p=fcc36eee94cb4c76a0b06f71d17bb360&pm=s) — a high-fidelity underwater
robotics simulation environment built on top of Gazebo and ROS. DAVE streams the vehicle's
telemetry over **MAVLink (UDP)**, which the GCS connects to just like a real vehicle.

There are two parts:

1. **Set up Project DAVE** — native on your host, or in Docker.
2. **Point the GCS at it** — which address you use depends on *where each side runs*.

---

## 1. Set up Project DAVE

Project DAVE can be installed two ways. Follow its official installation manual:

| Option | Description |
|--------|-------------|
| **A. Native (host install)** | Install ROS / Gazebo / dependencies directly on the host. Simplest if you're fine using the exact Ubuntu/ROS/Gazebo versions DAVE requires. |
| **B. Docker** | Run DAVE inside a container — leaves your host (mostly) untouched, good when juggling multiple ROS versions. |

→ Follow the official **Project DAVE installation guide** for the Native and Docker manuals
and the supported system requirements:
[**Project DAVE Installation (Notion)**](https://dave-ros2.notion.site/?v=d54cc8422868455888cc629d8e6117a9&p=fcc36eee94cb4c76a0b06f71d17bb360&pm=s)

{: .note }
> Make sure the simulator's MAVLink endpoint listens on **`0.0.0.0:14555`** (all interfaces),
> not only `127.0.0.1`. Otherwise it won't be reachable from another machine or container.

---

## 2. Which address do I use in the GCS?

The GCS connects to a MAVLink endpoint as `host:port` (default port **`14555`**). The right
**host** depends on where DAVE and the GCS each run:

| DAVE runs on | GCS runs on | Connect the GCS to |
|--------------|-------------|--------------------|
| Host (native) | Host (native) | `127.0.0.1:14555` |
| Host (native) | Docker | host gateway IP, e.g. `172.17.0.1:14555` — see [Quick Start (Docker)]({% link pages/quickstart-docker.md %}) |
| Docker | Host (native) | publish the port (`-p 14555:14555/udp`) → `127.0.0.1:14555` |
| **Docker** | **Docker** | put both on one Docker network → `dave:14555` (DAVE's container name) — see below |

Anything where one side is **native and the other is in Docker** is already covered by the
[Quick Start (Docker)]({% link pages/quickstart-docker.md %}) page (X11 with `xhost +local:docker`,
host gateway IP, etc.).

---

## 3. Docker ↔ Docker (both in containers)

When **both** DAVE and the GCS run in containers, they can't reach each other over `127.0.0.1` —
each container has its own loopback. Put them on a **shared Docker network** so they can talk by
**container name**.

### Quick way — a shared network

```bash
# 1. create a user-defined network
docker network create simnet

# 2. start DAVE on it, named "dave"
docker run -it --rm --name dave --network simnet <dave-image>   # DAVE's own run command + this network

# 3. start the GCS container on the same network
xhost +local:docker
docker run -it --rm --network simnet \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -v "$(pwd)":/workspace \
    ubuntu:24.04 bash
```

Inside the GCS container, install `libfuse2t64`, run the AppImage, and in the app connect to
**`dave:14555`** — the container name `dave` resolves to its IP on `simnet`.

### Cleaner way — docker-compose

A `docker-compose.yml` puts both services on one network automatically, and each service is
reachable by its **service name**:

```yaml
services:
  dave:
    image: <dave-image>          # from Project DAVE's Docker manual
    # ... DAVE's own settings (gpu, volumes, command) ...
    networks: [simnet]

  gcs:
    image: ubuntu:24.04
    depends_on: [dave]
    environment:
      - DISPLAY=${DISPLAY}
    volumes:
      - /tmp/.X11-unix:/tmp/.X11-unix:rw
      - ./:/workspace
    command: >
      bash -c "apt-get update && apt-get install -y libfuse2t64 &&
               /workspace/HolyUUV_GCS.AppImage"
    networks: [simnet]

networks:
  simnet:
```

```bash
xhost +local:docker     # let the containers open windows on your screen
docker compose up
```

In the GCS, connect to **`dave:14555`** (the service name).

{: .note }
> Prefer not to deal with container networking? Run **both** containers with `--network host`
> (or `network_mode: host` in compose) — then `127.0.0.1:14555` works as-is, at the cost of
> network isolation.

---

## 4. Launch the demo and connect

### Launch Project DAVE

Set up the environment and start the simulation. Run these in your terminal (native or inside the DAVE container):

```bash
export PATH=$PATH:/opt/ardusub_ws/ardupilot/build/sitl/bin
export GZ_SIM_SYSTEM_PLUGIN_PATH=$GZ_SIM_SYSTEM_PLUGIN_PATH:/opt/ardusub_ws/ardupilot_gazebo/build

ros2 launch dave_demos dave_robot.launch.py \
    z:=-0.5 \
    namespace:=bluerov2 \
    world_name:=dave_ocean_waves \
    paused:=false
```

### Launch HolyUUV GCS

In a separate terminal, run the AppImage:

```bash
chmod +x HolyUUV_GCS.AppImage
./HolyUUV_GCS.AppImage
```

### Connect the GCS to the simulator

Once ArduSub is up and streaming MAVLink, follow these steps in the GCS:

1. Click the **`sys_id`** card at the top-left to open the connections popup.

   ![Step 1]({{ '/assets/images/1.png' | relative_url }})

2. Under **CONNECTIONS**, click **＋ Add Connection**.

   ![Step 2]({{ '/assets/images/2.png' | relative_url }})

3. Choose **UDP**, set **Host** `127.0.0.1` and **Port** `14555` (the defaults), then **Add**.
   - For a remote vehicle or Docker setup, use the appropriate IP and port from the table in section 2 instead.

   ![Step 3]({{ '/assets/images/3.png' | relative_url }})

4. Back in the popup, press **Connect** next to the entry. As shown in the image, the vehicle
   will appear under the **VEHICLES** section. If it doesn't, the connection probably failed.
   To disconnect, just press **Disconnect**.

   ![Step 4]({{ '/assets/images/4.png' | relative_url }})

The **link card** at the top switches from `● NO LINKED` (red) to `● LINKED` (green) as soon as
the vehicle's HEARTBEAT is received.

## 5. Take control

Click the vehicle you want to inspect in the **VEHICLES** section — its information appears in
that vehicle's **CONTROL CENTER**, and the map pans over to its location. From there you can
**ARM** the vehicle and start piloting it. That's it — enjoy!

![Take control]({{ '/assets/images/5.png' | relative_url }})

