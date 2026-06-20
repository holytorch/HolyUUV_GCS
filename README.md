# HolyUUV_GCS v1.1.1

![HolyUUV GCS](assets/example.png)

**HolyUUV GCS** is a ground control station purpose-built for autonomous underwater vehicles (AUVs).
Designed from the ground up with underwater robotics in mind, it provides real-time telemetry monitoring,
vehicle control, and 3D terrain visualization — all in a clean, dark-themed interface.

This project is actively maintained and will continue to evolve with new features, improved stability,
and broader hardware support in future releases.

📖 **[Documentation](https://holytorch.github.io/HolyUUV_GCS/)** — installation, usage, simulator connection, and more.

---

## Features

- **Real-time telemetry** — attitude, depth, battery, GPS, heading, and link quality
- **Vehicle control** — arm/disarm, flight modes (Manual / Stabilize / Alt-Hold), and dual on-screen joysticks (MANUAL_CONTROL @ 50 Hz)
- **Multi-vehicle** — control several robots over one UDP port at once, told apart by MAVLink `sysid`; click a vehicle to switch control to it
- **3D terrain** — chunked terrain that follows the active vehicle, over OSM / satellite map tiles
- **File logging** — console output is mirrored to rotating log files at `~/.local/share/HolyUUV_GCS/logs/`, with crash backtraces captured
- **QGC-style UDP** — binds a fixed local port and receives from every sender (push model)

---

## Tech Stack

- **Qt 5** — UI framework (QML, Qt Location, Qt3D)
- **MAVLink** — vehicle communication protocol (ArduSub / ArduPilot compatible)

---

## Development

HolyUUV GCS was developed and validated using **Gazebo** simulation via **[Project DAVE](https://github.com/Field-Robotics-Lab/dave)**,
a high-fidelity underwater robotics simulation environment.

---

## Feature Status

> Feature status at a glance — this project is under active development.

| Feature | Status |
|---|---|
| Simulator connection (Gazebo / SITL) | ✅ Supported |
| Real hardware connection | 🔜 Planned |
| Single-vehicle control | ✅ Supported |
| Multi-vehicle support (simulator) | ✅ Supported |

Real hardware integration is on the roadmap and will be introduced in future updates.

---

## License

This project is licensed under the **Apache License 2.0** — see [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.
You may use, modify, and distribute it freely, provided you retain the original attribution.

This software uses **Qt 5**, which is licensed under the **GNU LGPL v3**.
Qt is dynamically linked, and its source is available at [qt.io](https://www.qt.io/).
**MAVLink** (c_library_v2) is licensed under the MIT License.
