#include "MavlinkManager.h"
#include "util/log/logger.h"

MavlinkManager::MavlinkManager(QObject* parent) : QObject(parent) {}

void MavlinkManager::parseBytes(const QByteArray& data)
{
#ifdef MAVLINK_AVAILABLE
    for (const char byte : data) {
        if (mavlink_parse_char(MAVLINK_COMM_0,
                               static_cast<uint8_t>(byte),
                               &_message, &_status))
        {
            switch (_message.msgid) {

                case MAVLINK_MSG_ID_HEARTBEAT: {
                    mavlink_heartbeat_t hb;
                    mavlink_msg_heartbeat_decode(&_message, &hb);
                    MavlinkHeartbeat out;
                    out.type         = hb.type;
                    out.autopilot    = hb.autopilot;
                    out.baseMode     = hb.base_mode;
                    out.customMode   = hb.custom_mode;
                    out.systemStatus = hb.system_status;
                    emit heartbeatReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_ATTITUDE: {
                    mavlink_attitude_t att;
                    mavlink_msg_attitude_decode(&_message, &att);
                    MavlinkAttitude out;
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
                    MavlinkSysStatus out;
                    out.voltageBattery   = sys.voltage_battery;
                    out.currentBattery   = sys.current_battery;
                    out.batteryRemaining = sys.battery_remaining;
                    emit sysStatusReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_SCALED_PRESSURE: {
                    mavlink_scaled_pressure_t sp;
                    mavlink_msg_scaled_pressure_decode(&_message, &sp);
                    MavlinkScaledPressure out;
                    out.pressureAbs  = sp.press_abs;
                    out.pressureDiff = sp.press_diff;
                    out.temperature  = sp.temperature;
                    emit scaledPressureReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_VFR_HUD: {
                    mavlink_vfr_hud_t hud;
                    mavlink_msg_vfr_hud_decode(&_message, &hud);
                    MavlinkVfrHud out;
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
                    out.lat         = pos.lat / 1e7;
                    out.lon         = pos.lon / 1e7;
                    out.alt         = pos.alt  / 1000.0f;
                    out.relativeAlt = pos.relative_alt / 1000.0f;
                    emit globalPositionReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_GPS_RAW_INT: {
                    mavlink_gps_raw_int_t gps;
                    mavlink_msg_gps_raw_int_decode(&_message, &gps);
                    MavlinkGpsRaw out;
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
