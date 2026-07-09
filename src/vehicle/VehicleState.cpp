#include "VehicleState.h"
#include <QTimer>
#include <QDebug>

// ArduSub flight-mode number → string table.
// Keyed by the customMode field value (ArduSub 4.x).
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
// Creates the heartbeat watchdog timer. If no heartbeat arrives for more than
// 3 seconds, _heartbeatOk is set to false and heartbeatStatusChanged is emitted.
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

    qInfo("[init] VehicleState");
}


// Binds the sysid at construction. Delegates to the default constructor to reuse
// the watchdog setup.
VehicleState::VehicleState(int sysid, QObject* parent)
    : VehicleState(parent)
{
    _sysid = sysid;
}


// ─────────────────────────────────────────────────────────────────────────────
// ~VehicleState()
// ─────────────────────────────────────────────────────────────────────────────
VehicleState::~VehicleState()
{
    qInfo("[exit] VehicleState");
}


// ─────────────────────────────────────────────────────────────────────────────
// updateBattery()
// Stores the parsed SYS_STATUS values and emits batteryChanged. current is
// received in cA (centiamperes) and converted (/100) for display.
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
// Stores the parsed ATTITUDE values (rad) and emits attitudeChanged.
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
// Stores the parsed VFR_HUD values and emits vfrHudChanged. depth is stored as the
// raw altitude (for an underwater vehicle, negative = depth; the UI flips the sign).
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
// Stores the parsed GLOBAL_POSITION_INT values and emits gpsChanged.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateGlobalPosition(double lat, double lon)
{
    _latitude  = lat;
    _longitude = lon;
    emit gpsChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateGpsRaw()
// Stores the parsed GPS_RAW_INT values and emits gpsChanged.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::updateGpsRaw(int satCount, float hdop)
{
    _gpsSatCount = satCount;
    _gpsHdop     = hdop;
    emit gpsChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// updateHeartbeat()
// Stores the parsed HEARTBEAT values.
// armed: baseMode & MAV_MODE_FLAG_SAFETY_ARMED (0x80)
// customMode: ArduSub flight-mode number → converted to a string
// Each message restarts _heartbeatElapsed so the watchdog can track connectivity.
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
// Called when MavlinkManager latches the sysid on the first HEARTBEAT. Also called
// when the user selects a different sysid in the tree, so the UI mirrors that vehicle.
// ─────────────────────────────────────────────────────────────────────────────
void VehicleState::setSysid(int sysid)
{
    if (_sysid == sysid) return;
    _sysid = sysid;
    emit sysidChanged();
}


// ─────────────────────────────────────────────────────────────────────────────
// resetTelemetry()
// Called on disconnect / active-vehicle switch. Restores all telemetry to defaults
// so no stale values remain (e.g. the previous vehicle's battery %).
// The sysid is managed separately via setSysid.
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
