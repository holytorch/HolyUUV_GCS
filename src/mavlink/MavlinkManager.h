#pragma once

#include <QObject>
#include <QByteArray>
#include <QSet>
#include <QMap>
#include <QTimer>
#include <cstdint>

#ifdef MAVLINK_AVAILABLE
#include <mavlink/v2.0/ardupilotmega/mavlink.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MAVLink 메시지 구조체
// C MAVLink 라이브러리 구조체를 Qt 타입으로 감싸서 시그널/슬롯에서 사용한다.
// ─────────────────────────────────────────────────────────────────────────────

// 다중로봇: 모든 텔레메트리 구조체에 source sysid를 실어 소비자(VehicleManager)가
// 어느 차량의 데이터인지 구분할 수 있게 한다. (RadioStatus는 모뎀 레벨이라 제외)
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

struct MavlinkVfrHud {
    int   sysid       = 0;
    float groundspeed = 0.0f;  // m/s
    float altitude    = 0.0f;  // m (수중 로봇은 음수 = 수심)
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
// LinkManager로부터 수신된 바이트 스트림을 MAVLink 패킷으로 파싱하고
// 메시지 종류별로 타입 안전한 시그널을 발신한다.
// MAVLINK_AVAILABLE이 정의되지 않으면 수신 데이터는 조용히 무시된다.
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
    // ArduSub custom_mode 변경. base_mode에 CUSTOM_MODE_ENABLED 비트 켜고
    // custom_mode 필드로 모드 번호 전달 (예: MANUAL=19, STABILIZE=0, ALT_HOLD=2).
    void sendSetMode(uint32_t customMode);
    // 연결 해제 시 호출. 감지된 sysid 목록과 active 모두 초기화.
    void resetVehicleLatch();
    // 사용자가 활성 차량 변경. HEARTBEAT/제어 송신의 target sysid가 된다.
    void setActiveSysid(int sysid);
    // 링크 연결/해제 시 호출. GCS HEARTBEAT 타이머와 차량 watchdog를 제어.
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

    // 새 sysid의 HEARTBEAT가 처음 수신될 때 발신 (UI 트리에 추가용).
    void sysidDetected(int sysid);
    // 활성 sysid 변경 (외부 setActiveSysid 또는 첫 자동 latch). HUD 리셋 트리거.
    void activeSysidChanged(int sysid);
    // 차량 HEARTBEAT가 5초 이상 수신되지 않으면 발신 → 자동 disconnect.
    void vehicleTimedOut();
    // 모든 sysid의 SYS_STATUS — 카드 슬롯 갱신용 (활성 필터 없음).
    void anyVehicleSysStatus(int sysid, int batteryRemaining, float voltage);

private:
#ifdef MAVLINK_AVAILABLE
    // 송신용 공통 헬퍼 — to_send_buffer + bytesToSend.
    void _emitMessage(mavlink_message_t& msg);

    mavlink_status_t  _status  = {};
    mavlink_message_t _message = {};
#endif

    // 디버깅: 첫 MANUAL_CONTROL 송신 로그용 (1회만)
    bool _loggedFirstManualControl = false;

    // 감지된 모든 sysid (HEARTBEAT 송신 차량). 한 연결에 여러 sysid가 섞일 수 있음.
    QSet<uint8_t> _detectedSysids;
    // sysid → compid 매핑 (HEARTBEAT 수신 시 갱신). setActiveSysid에서 target compid 결정에 사용.
    QMap<uint8_t, uint8_t> _sysidCompid;

    // 활성 차량 sysid — 0이면 미정. 첫 HEARTBEAT에서 자동 latch되거나,
    // 사용자가 setActiveSysid로 변경. 모든 송신의 target.
    uint8_t _activeSysid  = 0;
    uint8_t _targetCompid = 1;

    // GCS HEARTBEAT 1Hz 타이머 (ArduSub에게 GCS 존재 알림)
    QTimer* _gcsHeartbeatTimer  = nullptr;
    // 차량 HEARTBEAT 수신 watchdog — 5초 단발 타이머
    QTimer* _vehicleWatchdog    = nullptr;
};
