#include "VehicleState.h"

VehicleState::VehicleState(QObject* parent) : QObject(parent) {}

void VehicleState::updateBattery(int remaining, float voltage, float current)
{
    _batteryRemaining = remaining;
    _voltage          = voltage;
    _current          = current;
    emit batteryChanged();
}

void VehicleState::updateAttitude(float roll, float pitch, float yaw)
{
    _roll  = roll;
    _pitch = pitch;
    _yaw   = yaw;
    emit attitudeChanged();
}

void VehicleState::updateVfrHud(float groundspeed, float depth, float heading, int throttle)
{
    _groundspeed = groundspeed;
    _depth       = depth;
    _heading     = heading;
    _throttle    = throttle;
    emit vfrHudChanged();
}

void VehicleState::updateGlobalPosition(double lat, double lon)
{
    _latitude  = lat;
    _longitude = lon;
    emit gpsChanged();
}

void VehicleState::updateGpsRaw(int satCount, float hdop)
{
    _gpsSatCount = satCount;
    _gpsHdop     = hdop;
    emit gpsChanged();
}
