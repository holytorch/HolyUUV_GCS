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
void VehicleState::updateBattery(int remaining, float voltage, float current,
                                  uint16_t dropRate, uint16_t errorsComm)
{
    _batteryRemaining = remaining;
    _voltage          = voltage;
    _current          = current;
    _dropRateComm     = dropRate;
    _errorsComm       = errorsComm;
    emit batteryChanged();
    emit linkQualityChanged();
}

void VehicleState::updateRadioStatus(uint8_t rssi, uint8_t remRssi)
{
    _rssi           = rssi;
    _remRssi        = remRssi;
    _hasRadioStatus = true;
    emit linkQualityChanged();
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


// ─────────────────────────────────────────────────────────────────────────────
// setSysid()
// MavlinkManager가 첫 HEARTBEAT에서 sysid를 latch하면 호출.
// 사용자가 트리에서 다른 sysid 선택해도 호출되어 UI가 그 차량으로 mirror됨.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::setSysid(int sysid)
{
    if (_sysid == sysid) return;
    _sysid = sysid;
    emit sysidChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// resetTelemetry()
// 연결 해제 / 활성 차량 전환 시 호출. 모든 텔레메트리를 초기값으로 되돌려
// stale 표시(예: 이전 차량 batt %)가 남지 않도록 한다.
// sysid는 별도 setSysid로 관리.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::resetTelemetry()
{
    _batteryRemaining = -1; _voltage = 0; _current = 0;
    _roll = 0; _pitch = 0; _yaw = 0;
    _depth = 0; _groundspeed = 0; _heading = 0; _throttle = 0;
    _latitude = 0; _longitude = 0; _gpsSatCount = 0; _gpsHdop = 99.9f;
    _armed = false; _flightMode = "Unknown"; _heartbeatOk = false;
    _heartbeatElapsed.invalidate();
    _dropRateComm = 0; _errorsComm = 0;
    _rssi = 0; _remRssi = 0; _hasRadioStatus = false;

    emit batteryChanged();
    emit attitudeChanged();
    emit vfrHudChanged();
    emit gpsChanged();
    emit armedChanged();
    emit flightModeChanged();
    emit heartbeatStatusChanged();
    emit linkQualityChanged();
}
