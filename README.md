# HolyUUV_GCS v1.0.0

![HolyUUV GCS](assets/example.png)

**HolyUUV GCS** is a ground control station purpose-built for autonomous underwater vehicles (AUVs).
Designed from the ground up with underwater robotics in mind, it provides real-time telemetry monitoring,
vehicle control, and 3D terrain visualization — all in a clean, dark-themed interface.

This project is actively maintained and will continue to evolve with new features, improved stability,
and broader hardware support in future releases.

---

## Tech Stack

- **Qt 5** — UI framework (QML, Qt Location, Qt3D)
- **MAVLink** — vehicle communication protocol (ArduSub / ArduPilot compatible)

---

## Development

HolyUUV GCS was developed and validated using **Gazebo** simulation via **[Project DAVE](https://github.com/Field-Robotics-Lab/dave)**,
a high-fidelity underwater robotics simulation environment.

---

## Current Limitations

> This project is under active development. The following features are planned for upcoming releases.

| Feature | Status |
|---|---|
| Simulator connection (Gazebo / SITL) | ✅ Supported |
| Real hardware connection | 🔜 Planned |
| Multi-vehicle support | 🔜 Planned |
| Single-vehicle control | ✅ Supported |

Real hardware integration and multi-vehicle support are on the roadmap and will be introduced in future updates.
