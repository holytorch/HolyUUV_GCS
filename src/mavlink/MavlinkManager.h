#pragma once

#include <QObject>
#include <QByteArray>
#include <QSet>
#include <QMap>
#include <QHash>
#include <QTimer>
#include <QElapsedTimer>
#include <cstdint>

#ifdef MAVLINK_AVAILABLE
#include <mavlink/v2.0/ardupilotmega/mavlink.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MAVLink message structs
// Wrap the C MAVLink library structs in Qt-friendly types for use across
// signals/slots.
// ─────────────────────────────────────────────────────────────────────────────

// Multi-robot: every telemetry struct carries the source sysid so the consumer
// (VehicleManager) can tell which vehicle the data belongs to. (RadioStatus is
// modem-level and therefore excluded.)
struct MavlinkHeartbeat {
    int      sysid         = 0;
    uint8_t  type          = 0;
    uint8_t  autopilot     = 0;
    uint8_t  baseMode      = 0;
    uint32_t customMode    = 0;
    uint8_t  systemStatus  = 0;
};

struct MavlinkAttitude {
    int   sysid      = 0;
    float roll       = 0.0f;  // rad
    float pitch      = 0.0f;  // rad
    float yaw        = 0.0f;  // rad
    float rollSpeed  = 0.0f;  // rad/s
    float pitchSpeed = 0.0f;  // rad/s
    float yawSpeed   = 0.0f;  // rad/s
};

struct MavlinkSysStatus {
    int      sysid            = 0;
    uint16_t voltageBattery   = 0;    // mV
    int16_t  currentBattery   = -1;   // cA (-1 = unavailable)
    int8_t   batteryRemaining = -1;   // % (-1 = unavailable)
    uint16_t dropRateComm     = 0;    // packet loss rate % (0..10000 → /100 = %)
    uint16_t errorsComm       = 0;    // communication error count
};

struct MavlinkRadioStatus {
    uint8_t  rssi      = 0;    // local receive signal strength (0..254, 255=unknown)
    uint8_t  remRssi   = 0;    // remote signal strength
    uint8_t  noise     = 0;    // local noise
    uint8_t  remNoise  = 0;    // remote noise
    uint16_t rxErrors  = 0;    // receive error count
};

struct MavlinkVfrHud {
    int   sysid       = 0;
    float groundspeed = 0.0f;  // m/s
    float altitude    = 0.0f;  // m (negative for an underwater vehicle = depth)
    float heading     = 0.0f;  // deg
    int   throttle    = 0;     // %
};

struct MavlinkGlobalPosition {
    int    sysid       = 0;
    double lat         = 0.0;  // deg
    double lon         = 0.0;  // deg
    float  alt         = 0.0f; // m
    float  relativeAlt = 0.0f; // m
};

struct MavlinkGpsRaw {
    int   sysid    = 0;
    int   satCount = 0;
    float hdop     = 99.9f;
};

// ─────────────────────────────────────────────────────────────────────────────
// MavlinkManager
// Parses the byte stream received from LinkManager into MAVLink packets and emits
// a type-safe signal per message kind. When MAVLINK_AVAILABLE is not defined, all
// received data is silently ignored.
// ─────────────────────────────────────────────────────────────────────────────
class MavlinkManager : public QObject {
    Q_OBJECT
public:
    explicit MavlinkManager(QObject* parent = nullptr);
    ~MavlinkManager() override;

public slots:
    void parseBytes(const QByteArray& data);
    void sendManualControl(int16_t x, int16_t y, int16_t z, int16_t r, uint16_t buttons);
    void sendArmDisarm(bool arm);
    // Change the ArduSub custom_mode. Sets the CUSTOM_MODE_ENABLED bit in base_mode
    // and passes the mode number in the custom_mode field
    // (e.g. MANUAL=19, STABILIZE=0, ALT_HOLD=2).
    void sendSetMode(uint32_t customMode);
    // Called on disconnect. Resets both the detected-sysid set and the active sysid.
    void resetVehicleLatch();
    // The user changes the active vehicle. Becomes the target sysid for HEARTBEAT /
    // control transmissions.
    void setActiveSysid(int sysid);
    // Called on link connect/disconnect. Controls the GCS HEARTBEAT timer and the
    // vehicle watchdog.
    void startHeartbeat();
    void stopHeartbeat();

signals:
    void heartbeatReceived(const MavlinkHeartbeat& hb);
    void bytesToSend(const QByteArray& data);
    void attitudeReceived(const MavlinkAttitude& att);
    void sysStatusReceived(const MavlinkSysStatus& status);
    void vfrHudReceived(const MavlinkVfrHud& hud);
    void globalPositionReceived(const MavlinkGlobalPosition& pos);
    void gpsRawReceived(const MavlinkGpsRaw& gps);
    void radioStatusReceived(const MavlinkRadioStatus& radio);

    // Emitted when a HEARTBEAT from a new sysid is received for the first time
    // (to add it to the UI tree).
    void sysidDetected(int sysid);
    // Active sysid changed (external setActiveSysid or the first automatic latch).
    // Triggers a HUD reset.
    void activeSysidChanged(int sysid);
    // Emitted when a specific vehicle's (sysid) HEARTBEAT has been silent for a
    // while → remove only that vehicle (the link stays up).
    void vehicleTimedOut(int sysid);
    // SYS_STATUS from any sysid — for updating card slots (no active filter).
    void anyVehicleSysStatus(int sysid, int batteryRemaining, float voltage);

private:
    // Per-vehicle watchdog: periodically checks each sysid's last-HEARTBEAT time.
    void _sweepVehicles();

#ifdef MAVLINK_AVAILABLE
    // Shared transmit helper — to_send_buffer + bytesToSend.
    void _emitMessage(mavlink_message_t& msg);

    mavlink_status_t  _status  = {};
    mavlink_message_t _message = {};
#endif

    // Debug: logs the first MANUAL_CONTROL transmission (once only).
    bool _loggedFirstManualControl = false;

    // Every detected sysid (a HEARTBEAT-emitting vehicle). One connection may carry
    // several sysids.
    QSet<uint8_t> _detectedSysids;
    // sysid → compid mapping (updated on HEARTBEAT). Used by setActiveSysid to
    // resolve the target compid.
    QMap<uint8_t, uint8_t> _sysidCompid;

    // Active vehicle sysid — 0 means undetermined. Latched automatically on the
    // first HEARTBEAT, or changed by the user via setActiveSysid. Target of every
    // transmission.
    uint8_t _activeSysid  = 0;
    uint8_t _targetCompid = 1;

    // GCS HEARTBEAT 1 Hz timer (announces the GCS's presence to ArduSub).
    QTimer* _gcsHeartbeatTimer  = nullptr;

    // Per-vehicle watchdog: periodic sweep timer + last-HEARTBEAT time (ms) per sysid.
    QTimer*            _vehicleSweep = nullptr;
    QElapsedTimer      _uptime;                 // monotonic reference clock
    QHash<int, qint64> _lastSeen;               // sysid → last HEARTBEAT time (ms)
};
