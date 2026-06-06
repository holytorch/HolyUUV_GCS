---
title: Quick Start (Docker)
layout: default
nav_order: 2
---

# Quick Start (Docker)
{: .no_toc }

Run the prebuilt AppImage inside a Docker container — useful when you're on a different OS or
Ubuntu version and just want to run the app without building from source.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Prerequisites

- Docker installed on your host machine
- A display server running on the host (X11)
- *(GPU path only)* NVIDIA GPU with drivers installed on the host

---

## Without GPU

### 1. Allow Docker to access your display

Run this on the **host**, once per session:

```bash
xhost +local:docker
```

### 2. Download the latest AppImage

```bash
wget -O HolyUUV_GCS.AppImage \
  $(curl -s https://api.github.com/repos/holytorch/HolyUUV_GCS/releases/latest \
  | grep "browser_download_url.*AppImage" | cut -d'"' -f4)
chmod +x HolyUUV_GCS.AppImage
```

This always fetches the latest release automatically.

### 3. Launch the container

```bash
docker run -it --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -v "$(pwd)":/workspace \
    ubuntu:24.04 bash
```

### 4. Inside the container — verify workspace, install deps, and run

```bash
# confirm the AppImage is mounted
ls /workspace/HolyUUV_GCS.AppImage

# install libfuse2 (required by AppImage)
apt-get update && apt-get install -y libfuse2t64

# run
cd /workspace && ./HolyUUV_GCS.AppImage
```

{: .note }
> `libfuse2t64` is required because Ubuntu 24.04 ships FUSE 3 by default,
> but most AppImages still use the FUSE 2 interface.

---

## With GPU (NVIDIA)

### 1. Install NVIDIA Container Toolkit (host, once)

Follow the official installation guide:
[NVIDIA Container Toolkit — Install Guide](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)

### 2. Allow Docker to access your display

```bash
xhost +local:docker
```

### 3. Download the latest AppImage

```bash
wget -O HolyUUV_GCS.AppImage \
  $(curl -s https://api.github.com/repos/holytorch/HolyUUV_GCS/releases/latest \
  | grep "browser_download_url.*AppImage" | cut -d'"' -f4)
chmod +x HolyUUV_GCS.AppImage
```

### 4. Launch the container with GPU passthrough

```bash
docker run -it --rm \
    --gpus all \
    --runtime=nvidia \
    -e DISPLAY=$DISPLAY \
    -e NVIDIA_VISIBLE_DEVICES=all \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -v "$(pwd)":/workspace \
    ubuntu:24.04 bash
```

### 5. Inside the container — verify GPU, workspace, and run

```bash
# verify GPU is visible inside the container
nvidia-smi

# confirm the AppImage is mounted
ls /workspace/HolyUUV_GCS.AppImage

# install libfuse2
apt-get update && apt-get install -y libfuse2t64

# run
cd /workspace && ./HolyUUV_GCS.AppImage
```

{: .note }
> If `nvidia-smi` fails, make sure the NVIDIA Container Toolkit is correctly installed and
> `sudo systemctl restart docker` was run after configuration.

---

## Check connectivity with the host

The container needs to reach the host machine over the network to receive MAVLink UDP packets
(e.g. from a simulator running on the host).

From inside the container, find the host IP and test reachability:

```bash
# the default gateway is usually the host
HOST_IP=$(ip route | grep default | awk '{print $3}')
echo "Host IP: $HOST_IP"
ping -c 3 $HOST_IP
```

If the ping succeeds, MAVLink traffic on that IP and port (e.g. `14555`) will reach the GCS.
Set the connection in the app to `$HOST_IP:14555` instead of `127.0.0.1:14555`.

{: .note }
> Alternatively, launch the container with `--network=host` to share the host network directly —
> then `127.0.0.1` works as-is, but the container loses network isolation.
