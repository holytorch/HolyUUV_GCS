#include "VehicleState.h"

// ─────────────────────────────────────────────────────────────────────────────
// VehicleState()
// ─────────────────────────────────────────────────────────────────────────────
VehicleState::VehicleState(QObject* parent) : QObject(parent) {}


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
