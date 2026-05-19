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
    uint16_t dropRateComm     = 0;    // 패킷 손실률 % (0~10000 → /100 = %)
    uint16_t errorsComm       = 0;    // 통신 오류 횟수
};

struct MavlinkRadioStatus {
    uint8_t  rssi      = 0;    // 수신 신호 강도 (0~254, 255=unknown)
    uint8_t  remRssi   = 0;    // 원격 신호 강도
    uint8_t  noise     = 0;    // 로컬 노이즈
    uint8_t  remNoise  = 0;    // 원격 노이즈
    uint16_t rxErrors  = 0;    // 수신 오류 수
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
    void sendManualControl(int16_t x, int16_t y, int16_t z, int16_t r, uint16_t buttons);
    void sendArmDisarm(bool arm);

signals:
    void heartbeatReceived(const MavlinkHeartbeat& hb);
    void bytesToSend(const QByteArray& data);
    void attitudeReceived(const MavlinkAttitude& att);
    void sysStatusReceived(const MavlinkSysStatus& status);
    void scaledPressureReceived(const MavlinkScaledPressure& pressure);
    void vfrHudReceived(const MavlinkVfrHud& hud);
    void globalPositionReceived(const MavlinkGlobalPosition& pos);
    void gpsRawReceived(const MavlinkGpsRaw& gps);
    void radioStatusReceived(const MavlinkRadioStatus& radio);

private:
#ifdef MAVLINK_AVAILABLE
    // 송신용 공통 헬퍼 — to_send_buffer + bytesToSend.
    void _emitMessage(mavlink_message_t& msg);

    mavlink_status_t  _status  = {};
    mavlink_message_t _message = {};
#endif

    // 디버깅: 첫 HEARTBEAT / 첫 MANUAL_CONTROL 송신 로그용 (1회만)
    bool _loggedFirstHeartbeat     = false;
    bool _loggedFirstManualControl = false;

    // 첫 autopilot HEARTBEAT에서 latch — 모든 송신의 target sysid/compid.
    // 하드코딩 1로 두면 ArduSub이 다른 sysid일 때 명령 묵살.
    uint8_t _targetSysid  = 1;
    uint8_t _targetCompid = 1;
};
