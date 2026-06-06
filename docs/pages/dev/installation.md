---
title: Installation
layout: default
nav_order: 1
parent: For Developers (Build from Source)
---

# Installation
{: .no_toc }

Install dependencies and set up a working build environment.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Clone the repository

```bash
git clone https://github.com/holytorch/HolyUUV_GCS.git
cd HolyUUV_GCS
```

---

## Requirements

| Requirement | Notes                                            |
|-------------|--------------------------------------------------|
| OS          | Ubuntu 24.04 LTS (officially supported)          |
| Qt          | Qt 5 (installed by the dependency script)        |
| Build tools | CMake + a C++ toolchain                          |

## Install dependencies

Everything is driven by helper scripts in the
[`scripts/`](https://github.com/holytorch/HolyUUV_GCS/tree/main/scripts) folder.
Run from the project root:

```bash
./scripts/install_deps.sh
```

This installs all Qt 5 / build dependencies via `apt`, then runs `setup_mavlink.sh` to download
the MAVLink C headers (`c_library_v2`) into `3rdparty/`.

| Script                      | What it does                                                              |
|-----------------------------|--------------------------------------------------------------------------|
| `scripts/install_deps.sh`   | Installs Qt 5 / build dependencies via `apt`, then runs `setup_mavlink.sh` |
| `scripts/setup_mavlink.sh`  | Downloads the MAVLink C headers (`c_library_v2`) into `3rdparty/`         |

## Building on another OS (Docker)

On a different OS or Ubuntu version, spin up a clean Ubuntu 24.04 container and build inside it:

```bash
docker run -it --rm \
    --gpus all \                              # pass the GPU through (for 3D terrain)
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    -v "$(pwd)":/workspace \
    ubuntu:24.04 bash

# then, inside the container:
cd /workspace && ./scripts/install_deps.sh && cmake -B build && cmake --build build -j$(nproc)
```

{: .note }
> Run `xhost +local:docker` on the host first so the container can open windows.

---

Next: [Building from source]({% link pages/dev/building.md %}).
