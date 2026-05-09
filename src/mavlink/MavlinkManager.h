#pragma once

#include <QObject>
#include <QByteArray>
#include <cstdint>

#ifdef MAVLINK_AVAILABLE
#include <mavlink/v2.0/ardupilotmega/mavlink.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MAVLink 메시지 구조체
// C MAVLink 라이브러리 구조체를 Qt 타입으로 감싸서 시그널/슬롯에서 사용한다.
// ─────────────────────────────────────────────────────────────────────────────

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
    uint16_t voltageBattery   = 0;    // mV
    int16_t  currentBattery   = -1;   // cA (-1 = 사용 불가)
    int8_t   batteryRemaining = -1;   // % (-1 = 사용 불가)
};

struct MavlinkScaledPressure {
    float   pressureAbs  = 0.0f;  // hPa
    float   pressureDiff = 0.0f;  // hPa
    int16_t temperature  = 0;     // cdeg C
};

struct MavlinkVfrHud {
    float groundspeed = 0.0f;  // m/s
    float altitude    = 0.0f;  // m (수중 로봇은 음수 = 수심)
    float heading     = 0.0f;  // deg
    int   throttle    = 0;     // %
};

struct MavlinkGlobalPosition {
    double lat         = 0.0;  // deg
    double lon         = 0.0;  // deg
    float  alt         = 0.0f; // m
    float  relativeAlt = 0.0f; // m
};

struct MavlinkGpsRaw {
    int   satCount = 0;
    float hdop     = 99.9f;
};

// ─────────────────────────────────────────────────────────────────────────────
// MavlinkManager
// LinkManager로부터 수신된 바이트 스트림을 MAVLink 패킷으로 파싱하고
// 메시지 종류별로 타입 안전한 시그널을 발신한다.
// MAVLINK_AVAILABLE이 정의되지 않으면 수신 데이터는 조용히 무시된다.
// ─────────────────────────────────────────────────────────────────────────────
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
