#include "VehicleState.h"
#include <QTimer>

// ArduSub flight mode 번호 → 문자열 테이블
// customMode 필드 값 기준 (ArduSub 4.x 기준)
static const char* arduSubModeName(uint32_t mode)
{
    switch (mode) {
        case 0:  return "Stabilize";
        case 1:  return "Acro";
        case 2:  return "AltHold";
        case 3:  return "Auto";
        case 4:  return "Guided";
        case 7:  return "Circle";
        case 9:  return "Surface";
        case 16: return "PosHold";
        case 19: return "Manual";
        default: return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// VehicleState()
// heartbeat 감시 타이머를 생성한다. 3초 이상 heartbeat가 없으면
// _heartbeatOk를 false로 설정하고 heartbeatStatusChanged를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
VehicleState::VehicleState(QObject* parent) : QObject(parent)
{
    _watchdog = new QTimer(this);
    _watchdog->setInterval(500);
    connect(_watchdog, &QTimer::timeout, this, [this]() {
        const bool ok = _heartbeatElapsed.isValid() && _heartbeatElapsed.elapsed() < 3000;
        if (ok != _heartbeatOk) {
            _heartbeatOk = ok;
            emit heartbeatStatusChanged();
        }
    });
    _watchdog->start();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateBattery()
// SYS_STATUS 메시지 파싱 결과를 저장하고 batteryChanged를 발신한다.
// current는 cA(센티암페어) 단위로 수신되며 표시 시 /100으로 변환한다.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateBattery(int remaining, float voltage, float current)
{
    _batteryRemaining = remaining;
    _voltage          = voltage;
    _current          = current;
    emit batteryChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateAttitude()
// ATTITUDE 메시지 파싱 결과(rad)를 저장하고 attitudeChanged를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateAttitude(float roll, float pitch, float yaw)
{
    _roll  = roll;
    _pitch = pitch;
    _yaw   = yaw;
    emit attitudeChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateVfrHud()
// VFR_HUD 메시지 파싱 결과를 저장하고 vfrHudChanged를 발신한다.
// depth는 altitude 그대로 저장 (수중 로봇에서 음수 = 수심, UI에서 부호 반전).
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateVfrHud(float groundspeed, float depth, float heading, int throttle)
{
    _groundspeed = groundspeed;
    _depth       = depth;
    _heading     = heading;
    _throttle    = throttle;
    emit vfrHudChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateGlobalPosition()
// GLOBAL_POSITION_INT 메시지 파싱 결과를 저장하고 gpsChanged를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateGlobalPosition(double lat, double lon)
{
    _latitude  = lat;
    _longitude = lon;
    emit gpsChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateGpsRaw()
// GPS_RAW_INT 메시지 파싱 결과를 저장하고 gpsChanged를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateGpsRaw(int satCount, float hdop)
{
    _gpsSatCount = satCount;
    _gpsHdop     = hdop;
    emit gpsChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateHeartbeat()
// HEARTBEAT 메시지 파싱 결과를 저장한다.
// armed: baseMode & MAV_MODE_FLAG_SAFETY_ARMED (0x80)
// customMode: ArduSub 비행 모드 번호 → 문자열로 변환
// 수신마다 _heartbeatElapsed를 리셋해 watchdog가 연결 상태를 감지한다.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateHeartbeat(bool armed, uint32_t customMode)
{
    _heartbeatElapsed.restart();

    if (armed != _armed) {
        _armed = armed;
        emit armedChanged();
    }

    const QString newMode = QString::fromLatin1(arduSubModeName(customMode));
    if (newMode != _flightMode) {
        _flightMode = newMode;
        emit flightModeChanged();
    }

    if (!_heartbeatOk) {
        _heartbeatOk = true;
        emit heartbeatStatusChanged();
    }
}
