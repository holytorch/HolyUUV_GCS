#include "MavlinkManager.h"
#include "util/log/logger.h"
#include <QList>

// A vehicle whose HEARTBEAT has been silent for at least this long (ms) is timed
// out (only that vehicle is removed).
static constexpr qint64 kVehicleTimeoutMs = 5000;

// ─────────────────────────────────────────────────────────────────────────────
// MavlinkManager()
// ─────────────────────────────────────────────────────────────────────────────
MavlinkManager::MavlinkManager(QObject* parent) : QObject(parent)
{
    _gcsHeartbeatTimer = new QTimer(this);
    _gcsHeartbeatTimer->setInterval(1000);
    connect(_gcsHeartbeatTimer, &QTimer::timeout, this, [this]() {
#ifdef MAVLINK_AVAILABLE
        mavlink_message_t msg;
        mavlink_msg_heartbeat_pack(255, 0, &msg,
                                   MAV_TYPE_GCS,
                                   MAV_AUTOPILOT_INVALID,
                                   MAV_MODE_MANUAL_ARMED,
                                   0,
                                   MAV_STATE_ACTIVE);
        _emitMessage(msg);
#endif
    });

    // Per-vehicle watchdog — checks each vehicle's last HEARTBEAT once per second.
    _uptime.start();
    _vehicleSweep = new QTimer(this);
    _vehicleSweep->setInterval(1000);
    connect(_vehicleSweep, &QTimer::timeout, this, &MavlinkManager::_sweepVehicles);

    qInfo("[init] MavlinkManager");
}


// ─────────────────────────────────────────────────────────────────────────────
// ~MavlinkManager()
// ─────────────────────────────────────────────────────────────────────────────
MavlinkManager::~MavlinkManager()
{
    qInfo("[exit] MavlinkManager");
}


// ─────────────────────────────────────────────────────────────────────────────
// parseBytes()
// Feeds the received bytes into the MAVLink parser one byte at a time. When a
// complete packet is assembled, the corresponding signal is emitted based on the
// message ID. In builds where MAVLINK_AVAILABLE is not defined, the data is ignored.
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::parseBytes(const QByteArray& data)
{
#ifdef MAVLINK_AVAILABLE
    // MAVLink packet layout: [STX(1)][LEN(1)][SEQ(1)][SYS(1)][COMP(1)][MSGID(1)][PAYLOAD(LEN)][CRC(2)]
    // Why feed the parser one byte at a time:
    //   - a single UDP packet may contain several concatenated MAVLink messages
    //   - the parser accumulates bytes in its internal buffer and returns true once
    //     a packet is complete
    //   - STX (0xFD/0xFE) marks the start, LEN determines the end, CRC validates it
    for (const char byte : data) {
        // mavlink_parse_char: feeds one byte into the state machine.
        // Returns true once a packet is complete (STX..CRC all received + CRC passes).
        // Until then, _message is not touched and bytes keep accumulating.
        if (mavlink_parse_char(MAVLINK_COMM_0,
                               static_cast<uint8_t>(byte),
                               &_message, &_status))
        {
            // The MSGID (1 byte, 0..255) identifies the message kind.
            // Once the MSGID is known, the payload layout is fixed by the spec, so
            // mavlink_msg_xxx_decode() slices each field out by byte offset.
            switch (_message.msgid) {

                case MAVLINK_MSG_ID_HEARTBEAT: {
                    mavlink_heartbeat_t hb;
                    mavlink_msg_heartbeat_decode(&_message, &hb);

                    const uint8_t fromSysid = _message.sysid;
                    const bool isAutopilot  = (hb.autopilot != MAV_AUTOPILOT_INVALID);

                    // A relay (autopilot=INVALID) HEARTBEAT is excluded from vehicle
                    // detection. No automatic latch — the active vehicle is set via
                    // setActiveSysid when the user clicks a VEHICLES card.
                    if (isAutopilot) {
                        _sysidCompid[fromSysid] = _message.compid;
                        if (!_detectedSysids.contains(fromSysid)) {
                            _detectedSysids.insert(fromSysid);
                            LOG_INFO("Detected vehicle: sysid=%d compid=%d type=%d base=0x%02X custom=%u",
                                     fromSysid, _message.compid,
                                     hb.type, hb.base_mode, hb.custom_mode);
                            emit sysidDetected(static_cast<int>(fromSysid));
                        }
                    }

                    // Record the autopilot HEARTBEAT arrival time (for the per-vehicle watchdog).
                    if (isAutopilot)
                        _lastSeen[fromSysid] = _uptime.elapsed();

                    // Forward every vehicle's HEARTBEAT tagged with its sysid (VehicleManager routes it).
                    if (isAutopilot) {
                        MavlinkHeartbeat out;
                        out.sysid        = fromSysid;
                        out.type         = hb.type;
                        out.autopilot    = hb.autopilot;
                        out.baseMode     = hb.base_mode;
                        out.customMode   = hb.custom_mode;
                        out.systemStatus = hb.system_status;
                        emit heartbeatReceived(out);
                    }
                    break;
                }

                case MAVLINK_MSG_ID_ATTITUDE: {
                    mavlink_attitude_t att;
                    mavlink_msg_attitude_decode(&_message, &att);
                    MavlinkAttitude out;
                    out.sysid      = _message.sysid;
                    out.roll       = att.roll;
                    out.pitch      = att.pitch;
                    out.yaw        = att.yaw;
                    out.rollSpeed  = att.rollspeed;
                    out.pitchSpeed = att.pitchspeed;
                    out.yawSpeed   = att.yawspeed;
                    emit attitudeReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_SYS_STATUS: {
                    mavlink_sys_status_t sys;
                    mavlink_msg_sys_status_decode(&_message, &sys);

                    // Every sysid → card slot (no active filter)
                    emit anyVehicleSysStatus(
                        static_cast<int>(_message.sysid),
                        static_cast<int>(static_cast<int8_t>(sys.battery_remaining)),
                        sys.voltage_battery / 1000.0f);

                    // Forward every vehicle's detailed status tagged with its sysid (VehicleManager routes it)
                    {
                        MavlinkSysStatus out;
                        out.sysid            = _message.sysid;
                        out.voltageBattery   = sys.voltage_battery;
                        out.currentBattery   = sys.current_battery;
                        out.batteryRemaining = sys.battery_remaining;
                        out.dropRateComm     = sys.drop_rate_comm;  // SITL: always 0
                        out.errorsComm       = sys.errors_comm;
                        emit sysStatusReceived(out);
                    }
                    break;
                }

                case MAVLINK_MSG_ID_RADIO_STATUS: {
                    // RADIO_STATUS is sent by the radio modem (sysid 0 or 51). No active-vehicle filter.
                    mavlink_radio_status_t radio;
                    mavlink_msg_radio_status_decode(&_message, &radio);
                    MavlinkRadioStatus out;
                    out.rssi     = radio.rssi;
                    out.remRssi  = radio.remrssi;
                    out.noise    = radio.noise;
                    out.remNoise = radio.remnoise;
                    out.rxErrors = radio.rxerrors;
                    emit radioStatusReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_VFR_HUD: {
                    mavlink_vfr_hud_t hud;
                    mavlink_msg_vfr_hud_decode(&_message, &hud);
                    MavlinkVfrHud out;
                    out.sysid       = _message.sysid;
                    out.groundspeed = hud.groundspeed;
                    out.altitude    = hud.alt;
                    out.heading     = static_cast<float>(hud.heading);
                    out.throttle    = hud.throttle;
                    emit vfrHudReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
                    mavlink_global_position_int_t pos;
                    mavlink_msg_global_position_int_decode(&_message, &pos);
                    MavlinkGlobalPosition out;
                    out.sysid       = _message.sysid;
                    out.lat         = pos.lat / 1e7;
                    out.lon         = pos.lon / 1e7;
                    out.alt         = pos.alt / 1000.0f;
                    out.relativeAlt = pos.relative_alt / 1000.0f;
                    emit globalPositionReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_COMMAND_ACK: {
                    mavlink_command_ack_t ack;
                    mavlink_msg_command_ack_decode(&_message, &ack);
                    const char* resStr =
                        ack.result == MAV_RESULT_ACCEPTED            ? "ACCEPTED" :
                        ack.result == MAV_RESULT_TEMPORARILY_REJECTED ? "TEMP_REJECTED" :
                        ack.result == MAV_RESULT_DENIED              ? "DENIED" :
                        ack.result == MAV_RESULT_UNSUPPORTED         ? "UNSUPPORTED" :
                        ack.result == MAV_RESULT_FAILED              ? "FAILED" :
                        ack.result == MAV_RESULT_IN_PROGRESS         ? "IN_PROGRESS" : "?";
                    LOG_INFO("COMMAND_ACK: cmd=%u result=%u (%s)",
                             ack.command, ack.result, resStr);
                    break;
                }

                case MAVLINK_MSG_ID_GPS_RAW_INT: {
                    mavlink_gps_raw_int_t gps;
                    mavlink_msg_gps_raw_int_decode(&_message, &gps);
                    MavlinkGpsRaw out;
                    out.sysid    = _message.sysid;
                    out.satCount = gps.satellites_visible;
                    out.hdop     = gps.eph / 100.0f;
                    emit gpsRawReceived(out);
                    break;
                }

                default:
                    break;
            }
        }
    }
#else
    Q_UNUSED(data)
#endif
}


#ifdef MAVLINK_AVAILABLE
// ─────────────────────────────────────────────────────────────────────────────
// _emitMessage()
// Serializes a built mavlink_message_t into a byte buffer and emits it via
// bytesToSend. Shared by all transmit functions (sendArmDisarm, sendManualControl, ...).
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::_emitMessage(mavlink_message_t& msg)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    emit bytesToSend(QByteArray(reinterpret_cast<const char*>(buf),
                                static_cast<int>(len)));
}
#endif


// ─────────────────────────────────────────────────────────────────────────────
// sendArmDisarm()
// Sends MAV_CMD_COMPONENT_ARM_DISARM (cmd=400) as a COMMAND_LONG. Same approach as
// QGC — MANUAL_CONTROL button bits would need a BTN_n_FUNCTION mapping, whereas
// ArduSub always acts on a COMMAND_LONG.
//
//   param1: 1.0 = arm,  0.0 = disarm
//   param2: 0     = arm after passing safety checks,  21196 = force
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::sendArmDisarm(bool arm)
{
#ifdef MAVLINK_AVAILABLE
    LOG_INFO("Send ARM_DISARM → target %d:%d : %s (force)",
             _activeSysid, _targetCompid, arm ? "ARM" : "DISARM");

    // param2 = 21196 (magic value, same as QGC's "Force Arm"): bypasses the
    // pre-arm checks, so arming is possible in SITL/dev even without a GPS lock or a
    // converged EKF. A dedicated safety policy should be considered for real vehicles.
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(
        255, 0, &msg,                     // GCS sysid, compid
        _activeSysid, _targetCompid,      // target = value latched from HEARTBEAT
        MAV_CMD_COMPONENT_ARM_DISARM,
        0,                                // confirmation
        arm ? 1.0f : 0.0f,                // param1
        arm ? 21196.0f : 0.0f,            // param2: 21196 = force when arming
        0, 0, 0, 0, 0                     // param3..7 unused
    );
    _emitMessage(msg);
#else
    Q_UNUSED(arm);
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
// sendManualControl()
// Builds a MAVLink MANUAL_CONTROL packet and emits it via bytesToSend. The target
// uses the sysid latched from HEARTBEAT. Called at 50 Hz.
// x/y/r: [-1000, 1000], z: [0, 1000] (500 = neutral)
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::sendManualControl(int16_t x, int16_t y, int16_t z,
                                        int16_t r, uint16_t buttons)
{
#ifdef MAVLINK_AVAILABLE
    if (!_loggedFirstManualControl) {
        _loggedFirstManualControl = true;
        LOG_INFO("First MANUAL_CONTROL → target sysid=%d  x=%d y=%d z=%d r=%d btns=0x%04X",
                 _activeSysid, x, y, z, r, buttons);
    }

    mavlink_message_t msg;
    // buttons2 / enabled_extensions / aux1..6 / s / t: unused, so 0
    mavlink_msg_manual_control_pack(255, 0, &msg,
                                    _activeSysid,
                                    x, y, z, r,
                                    buttons,
                                    0, 0,                  // buttons2, enabled_extensions
                                    0, 0, 0, 0, 0, 0, 0, 0); // s, t, aux1..6
    _emitMessage(msg);
#else
    Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(z); Q_UNUSED(r); Q_UNUSED(buttons);
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
// startHeartbeat() / stopHeartbeat()
// Connected to linkConnected/linkDisconnected. Controls the GCS HEARTBEAT timer
// and the vehicle watchdog together.
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::startHeartbeat()
{
    _gcsHeartbeatTimer->start();
    _vehicleSweep->start();
    LOG_INFO("GCS heartbeat started (1Hz), per-vehicle watchdog started (%llds)",
             static_cast<long long>(kVehicleTimeoutMs / 1000));
}

void MavlinkManager::stopHeartbeat()
{
    _gcsHeartbeatTimer->stop();
    _vehicleSweep->stop();
    _lastSeen.clear();
}


// Per-vehicle watchdog sweep: removes from tracking any vehicle whose last
// HEARTBEAT is older than kVehicleTimeoutMs and emits vehicleTimedOut(sysid)
// (the link stays up).
void MavlinkManager::_sweepVehicles()
{
    const qint64 now = _uptime.elapsed();
    QList<int> timedOut;
    for (auto it = _lastSeen.constBegin(); it != _lastSeen.constEnd(); ++it)
        if (now - it.value() > kVehicleTimeoutMs)
            timedOut.append(it.key());

    for (int sysid : timedOut) {
        LOG_INFO("Vehicle %d heartbeat timeout → removed (link stays up)", sysid);
        _lastSeen.remove(sysid);
        _detectedSysids.remove(static_cast<uint8_t>(sysid));
        _sysidCompid.remove(static_cast<uint8_t>(sysid));
        emit vehicleTimedOut(sysid);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// resetVehicleLatch()
// Called on disconnect. Resets both the detected-sysid set and the active sysid,
// so the next connection's first HEARTBEAT starts the latch process again.
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::resetVehicleLatch()
{
    _detectedSysids.clear();
    _sysidCompid.clear();
    _lastSeen.clear();
    if (_activeSysid != 0) {
        _activeSysid = 0;
        emit activeSysidChanged(0);
    }
    _loggedFirstManualControl = false;
}


// ─────────────────────────────────────────────────────────────────────────────
// setActiveSysid()
// Called when the user clicks a different sysid in the tree. 0 clears the active
// vehicle. A sysid not in the detected set may still be set (pending state).
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::setActiveSysid(int sysid)
{
    const uint8_t s = static_cast<uint8_t>(sysid);
    if (s == _activeSysid) return;
    _activeSysid = s;
    if (s != 0)
        _targetCompid = _sysidCompid.value(s, 1);   // known compid, or ArduSub's default of 1
    LOG_INFO("Active sysid switched to %d (compid=%d)", _activeSysid, _targetCompid);
    emit activeSysidChanged(sysid);
}


// ─────────────────────────────────────────────────────────────────────────────
// sendSetMode()
// Changes ArduSub's flight mode via a MAVLink SET_MODE message. Sets the
// MAV_MODE_FLAG_CUSTOM_MODE_ENABLED bit in base_mode and passes the mode number in
// custom_mode.
//
// ArduSub custom_mode values:
//   0=STABILIZE  1=ACRO  2=ALT_HOLD  3=AUTO  4=GUIDED  7=CIRCLE
//   9=SURFACE  16=POSHOLD  19=MANUAL  20=MOTOR_DETECT
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::sendSetMode(uint32_t customMode)
{
#ifdef MAVLINK_AVAILABLE
    LOG_INFO("Send SET_MODE → target %d : custom_mode=%u",
             _activeSysid, customMode);

    mavlink_message_t msg;
    mavlink_msg_set_mode_pack(
        255, 0, &msg,                               // GCS sysid, compid
        _activeSysid,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,           // base_mode (0x01)
        customMode                                   // custom_mode
    );
    _emitMessage(msg);
#else
    Q_UNUSED(customMode);
#endif
}
