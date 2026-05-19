#include "MavlinkManager.h"
#include "util/log/logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// MavlinkManager()
// ─────────────────────────────────────────────────────────────────────────────
MavlinkManager::MavlinkManager(QObject* parent) : QObject(parent) {}


// ─────────────────────────────────────────────────────────────────────────────
// parseBytes()
// 수신 바이트를 한 바이트씩 MAVLink 파서에 공급한다.
// 완전한 패킷이 완성되면 메시지 ID에 따라 해당 시그널을 발신한다.
// MAVLINK_AVAILABLE이 정의되지 않은 빌드에서는 데이터를 무시한다.
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::parseBytes(const QByteArray& data)
{
#ifdef MAVLINK_AVAILABLE
    // MAVLink 패킷 구조: [STX(1byte)][LEN(1)][SEQ(1)][SYS(1)][COMP(1)][MSGID(1)][PAYLOAD(LEN)][CRC(2)]
    // 1바이트씩 파서에 공급하는 이유:
    //   - UDP 패킷 하나에 MAVLink 메시지 여러 개가 붙어올 수 있음
    //   - 파서가 내부 버퍼에 바이트를 쌓다가 패킷이 완성되면 true 반환
    //   - STX(0xFD/0xFE) 로 시작점 감지, LEN 으로 끝점 계산, CRC 로 유효성 검증
    for (const char byte : data) {
        // mavlink_parse_char: 바이트 1개를 상태머신에 공급
        // 패킷이 완성(STX~CRC 전부 수신 + CRC 통과)되면 true 반환
        // true 전까지는 _message에 접근하지 않고 계속 바이트를 쌓음
        if (mavlink_parse_char(MAVLINK_COMM_0,
                               static_cast<uint8_t>(byte),
                               &_message, &_status))
        {
            // MSGID(1바이트, 0~255)로 메시지 종류 식별
            // MSGID가 정해지면 페이로드 구조가 스펙에 고정되어 있어서
            // mavlink_msg_xxx_decode()가 바이트 위치 기준으로 각 필드를 잘라 해석함
            switch (_message.msgid) {

                case MAVLINK_MSG_ID_HEARTBEAT: {
                    mavlink_heartbeat_t hb;
                    mavlink_msg_heartbeat_decode(&_message, &hb);

                    const bool isAutopilot = (hb.autopilot != MAV_AUTOPILOT_INVALID);

                    // 첫 autopilot HEARTBEAT에서만 target latch (이후 다른 sysid로 안 바뀜).
                    // mavlink-router 같은 중계기가 자기 HEARTBEAT를 끼워보내도
                    // autopilot이 아니면 무시.
                    if (isAutopilot && !_loggedFirstHeartbeat) {
                        _targetSysid       = _message.sysid;
                        _targetCompid      = _message.compid;
                        _loggedFirstHeartbeat = true;
                        LOG_INFO("First HEARTBEAT (autopilot): sysid=%d compid=%d type=%d base=0x%02X custom=%u → target locked",
                                 _targetSysid, _targetCompid,
                                 hb.type, hb.base_mode, hb.custom_mode);
                    }

                    // VehicleState로는 latch된 target의 HEARTBEAT만 전달.
                    // 중계기 heartbeat(armed=0)와 autopilot heartbeat(armed=1)이
                    // 번갈아 도착하면 ARMED/DISARMED 깜빡임 발생 → 여기서 차단.
                    if (_loggedFirstHeartbeat &&
                        _message.sysid  == _targetSysid &&
                        _message.compid == _targetCompid) {
                        MavlinkHeartbeat out;
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
                    out.dropRateComm     = sys.drop_rate_comm;  // SITL: 항상 0
                    out.errorsComm       = sys.errors_comm;
                    emit sysStatusReceived(out);
                    break;
                }

                case MAVLINK_MSG_ID_RADIO_STATUS: {
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
// 빌드된 mavlink_message_t를 바이트 버퍼로 직렬화해 bytesToSend로 발신한다.
// 모든 송신 함수(sendArmDisarm, sendManualControl, ...)가 공유.
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
// MAV_CMD_COMPONENT_ARM_DISARM (cmd=400)을 COMMAND_LONG으로 송신한다.
// QGC와 동일한 방식 — MANUAL_CONTROL 비트마스크는 BTN_n_FUNCTION 매핑이 필요하지만,
// COMMAND_LONG은 ArduSub이 무조건 처리한다.
//
//   param1: 1.0 = arm,  0.0 = disarm
//   param2: 0     = 안전체크 통과 후 무장,  21196 = 강제(force)
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::sendArmDisarm(bool arm)
{
#ifdef MAVLINK_AVAILABLE
    LOG_INFO("Send ARM_DISARM → target %d:%d : %s",
             _targetSysid, _targetCompid, arm ? "ARM" : "DISARM");

    mavlink_message_t msg;
    mavlink_msg_command_long_pack(
        255, 0, &msg,                     // GCS sysid, compid
        _targetSysid, _targetCompid,      // target = HEARTBEAT에서 latch된 값
        MAV_CMD_COMPONENT_ARM_DISARM,
        0,                                // confirmation
        arm ? 1.0f : 0.0f,                // param1
        0, 0, 0, 0, 0, 0                  // param2~7 unused
    );
    _emitMessage(msg);
#else
    Q_UNUSED(arm);
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
// sendManualControl()
// MAVLink MANUAL_CONTROL 패킷을 빌드하고 bytesToSend 신호로 발신한다.
// target은 HEARTBEAT에서 latch된 값 사용. 50 Hz로 호출됨.
// x/y/r: [-1000, 1000], z: [0, 1000] (500=중립)
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::sendManualControl(int16_t x, int16_t y, int16_t z,
                                        int16_t r, uint16_t buttons)
{
#ifdef MAVLINK_AVAILABLE
    if (!_loggedFirstManualControl) {
        _loggedFirstManualControl = true;
        LOG_INFO("First MANUAL_CONTROL → target sysid=%d  x=%d y=%d z=%d r=%d btns=0x%04X",
                 _targetSysid, x, y, z, r, buttons);
    }

    mavlink_message_t msg;
    // buttons2/enabled_extensions/aux1~6/s/t: 사용하지 않으므로 0
    mavlink_msg_manual_control_pack(255, 0, &msg,
                                    _targetSysid,
                                    x, y, z, r,
                                    buttons,
                                    0, 0,                  // buttons2, enabled_extensions
                                    0, 0, 0, 0, 0, 0, 0, 0); // s, t, aux1~6
    _emitMessage(msg);
#else
    Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(z); Q_UNUSED(r); Q_UNUSED(buttons);
#endif
}
