---
title: Home
layout: home
nav_order: 1
description: "HolyUUV GCS — a ground control station for autonomous underwater vehicles (AUVs)."
permalink: /
---

# HolyUUV GCS
{: .fs-9 }

A ground control station purpose-built for autonomous underwater vehicles (AUVs) —
real-time telemetry, vehicle control, and 3D terrain visualization in a clean, dark-themed interface.
{: .fs-6 .fw-300 }

[⬇ Download AppImage](https://github.com/holytorch/HolyUUV_GCS/releases/latest/download/HolyUUV_GCS.AppImage){: .btn .btn-green .fs-5 .mb-4 .mb-md-0 .mr-2 }
[Get started]({% link pages/connecting.md %}){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[View on GitHub](https://github.com/holytorch/HolyUUV_GCS){: .btn .fs-5 .mb-4 .mb-md-0 }

---

![HolyUUV GCS]({{ '/assets/images/example.png' | relative_url }})

## Download

The quickest way to run HolyUUV GCS is the prebuilt **AppImage** — a single portable file,
no installation required.

- ⬇ **[Latest AppImage](https://github.com/holytorch/HolyUUV_GCS/releases/latest/download/HolyUUV_GCS.AppImage)** — always points to the newest release
- 📦 [All releases](https://github.com/holytorch/HolyUUV_GCS/releases) — pick a specific version

```bash
chmod +x HolyUUV_GCS.AppImage
./HolyUUV_GCS.AppImage
```

Prefer to build it yourself? See [Installation]({% link pages/installation.md %}) and [Building]({% link pages/building.md %}).

## Supported platform

{: .warning }
> Officially supported on **Ubuntu Linux 24.04 (LTS)** only.

| | |
|---|---|
| **OS** | Ubuntu 24.04 LTS — developed & tested here |
| **AppImage runtime** | Ubuntu **24.04 or newer** (built against glibc 2.39) |
| **GPU** | Optional — hardware-accelerated 3D when available, software fallback otherwise |

Other distributions or Ubuntu versions are not guaranteed to work, mainly due to differing
Qt 5 / glibc versions. On a different OS, build inside a Ubuntu 24.04
[Docker container]({% link pages/installation.md %}#building-on-another-os-docker).

## What's in this guide

- [**Installation**]({% link pages/installation.md %}) — dependencies and build environment
- [**Building**]({% link pages/building.md %}) — build from source, package an AppImage
- [**Connecting**]({% link pages/connecting.md %}) — connect to a Gazebo simulator or vehicle over MAVLink
- [**Interface**]({% link pages/interface.md %}) — every card, gauge, and control on screen
- [**Joystick & control**]({% link pages/joystick.md %}) — drive the vehicle, ARM, flight modes
- [**Telemetry**]({% link pages/telemetry.md %}) — what MAVLink data the station renders

## License

Released under the [MIT License](https://github.com/holytorch/HolyUUV_GCS/blob/main/LICENSE).
Uses Qt 5 (LGPL v3, dynamically linked) and MAVLink `c_library_v2` (MIT).
