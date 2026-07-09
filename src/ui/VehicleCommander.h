#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// VehicleCommander
// A QML → MAVLink transmit bridge. When QML (or other C++ code) requests
// arm/disarm/setMode via a Q_INVOKABLE method, it forwards the request as a signal.
// Application connects these signals to MavlinkManager's send* slots.
//
// Responsible for transmission only. The current state (armed, current mode) is
// read from VehicleState.
// ─────────────────────────────────────────────────────────────────────────────
class VehicleCommander : public QObject {
    Q_OBJECT
public:
    explicit VehicleCommander(QObject* parent = nullptr);
    ~VehicleCommander() override;

    Q_INVOKABLE void setArm(bool arm);
    // ArduSub mode name. Recognized values: STABILIZE, ACRO, ALT_HOLD, AUTO, GUIDED,
    // CIRCLE, SURFACE, POSHOLD, MANUAL, MOTOR_DETECT (case-insensitive).
    Q_INVOKABLE void setMode(const QString& modeName);
    // QML joystick → MAVLink MANUAL_CONTROL. Recommended to call at 50 Hz.
    // x/y/r: [-1000, 1000], z: [0, 1000] (500 = neutral). buttons is a bitmask.
    Q_INVOKABLE void sendManualControl(int x, int y, int z, int r, int buttons);

signals:
    void armRequested(bool arm);
    void setModeRequested(uint32_t customMode);
    void manualControlRequested(int16_t x, int16_t y, int16_t z, int16_t r, uint16_t buttons);
};
