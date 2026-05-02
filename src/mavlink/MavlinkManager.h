#pragma once

#include <QObject>
#include <QByteArray>
#include <cstdint>

#ifdef MAVLINK_AVAILABLE
#include <mavlink/v2.0/ardupilotmega/mavlink.h>
#endif

struct MavlinkHeartbeat {
    uint8_t  type          = 0;
    uint8_t  autopilot     = 0;
    uint8_t  baseMode      = 0;
    uint32_t customMode    = 0;
    uint8_t  systemStatus  = 0;
};

struct MavlinkAttitude {
    float roll       = 0.0f;  // rad
    float pitch      = 0.0f;  // rad
    float yaw        = 0.0f;  // rad
    float rollSpeed  = 0.0f;  // rad/s
    float pitchSpeed = 0.0f;  // rad/s
    float yawSpeed   = 0.0f;  // rad/s
};

struct MavlinkSysStatus {
    uint16_t voltageBattery  = 0;   // mV
    int16_t  currentBattery  = -1;  // cA (-1 = unavailable)
    int8_t   batteryRemaining = -1; // % (-1 = unavailable)
};

struct MavlinkScaledPressure {
    float   pressureAbs  = 0.0f;  // hPa
    float   pressureDiff = 0.0f;  // hPa
    int16_t temperature  = 0;     // cdeg C
};

struct MavlinkVfrHud {
    float   groundspeed = 0.0f;  // m/s
    float   altitude    = 0.0f;  // m (수중로봇은 음수 = 수심)
    float   heading     = 0.0f;  // deg
    int     throttle    = 0;     // %
};

struct MavlinkGlobalPosition {
    double  lat = 0.0;  // deg
    double  lon = 0.0;  // deg
    float   alt = 0.0f; // m
    float   relativeAlt = 0.0f; // m
};

struct MavlinkGpsRaw {
    int     satCount = 0;
    float   hdop     = 99.9f;
};

class MavlinkManager : public QObject {
    Q_OBJECT
public:
    explicit MavlinkManager(QObject* parent = nullptr);

public slots:
    void parseBytes(const QByteArray& data);

signals:
    void heartbeatReceived(const MavlinkHeartbeat& hb);
    void attitudeReceived(const MavlinkAttitude& att);
    void sysStatusReceived(const MavlinkSysStatus& status);
    void scaledPressureReceived(const MavlinkScaledPressure& pressure);
    void vfrHudReceived(const MavlinkVfrHud& hud);
    void globalPositionReceived(const MavlinkGlobalPosition& pos);
    void gpsRawReceived(const MavlinkGpsRaw& gps);

private:
#ifdef MAVLINK_AVAILABLE
    mavlink_status_t  _status  = {};
    mavlink_message_t _message = {};
#endif
};
