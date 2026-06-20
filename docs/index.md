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

[⬇ Download AppImage](https://github.com/holytorch/HolyUUV_GCS/releases/latest){: .btn .btn-blue .fs-5 .mb-4 .mb-md-0 .mr-2 }
[View on GitHub](https://github.com/holytorch/HolyUUV_GCS){: .btn .fs-5 .mb-4 .mb-md-0 }

---

![HolyUUV GCS]({{ '/assets/images/example.png' | relative_url }})

## Quick Start (local)

Just download the **AppImage** and run it — no installation needed.

- 📦 [All releases](https://github.com/holytorch/HolyUUV_GCS/releases)

```bash
wget -O HolyUUV_GCS.AppImage \
  $(curl -s https://api.github.com/repos/holytorch/HolyUUV_GCS/releases/latest \
  | grep "browser_download_url.*AppImage" | cut -d'"' -f4)
chmod +x HolyUUV_GCS.AppImage
./HolyUUV_GCS.AppImage
```

The app will automatically detect and use your GPU for hardware-accelerated 3D terrain rendering.
No GPU? No problem — it falls back to software rendering on its own, nothing to configure.

Want to quickly test with a simulated underwater robot? → [GCS + Simulator]({% link pages/simulation.md %})

## License

Released under the [Apache License 2.0](https://github.com/holytorch/HolyUUV_GCS/blob/main/LICENSE) — free to use, modify, and distribute with attribution.
Uses Qt 5 (LGPL v3, dynamically linked) and MAVLink `c_library_v2` (MIT).

