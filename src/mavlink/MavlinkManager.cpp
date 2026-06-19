#include "MavlinkManager.h"
#include "util/log/logger.h"
#include <QList>

// 차량 HEARTBEAT가 이 시간(ms) 이상 끊기면 타임아웃 처리 (그 차량만 제거).
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

    // per-vehicle 워치독 — 1초마다 각 차량의 마지막 HEARTBEAT를 점검.
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

                    const uint8_t fromSysid = _message.sysid;
                    const bool isAutopilot  = (hb.autopilot != MAV_AUTOPILOT_INVALID);

                    // 중계기(autopilot=INVALID) HEARTBEAT는 차량 감지에서 제외.
                    // 자동 latch는 하지 않음 — 사용자가 VEHICLES 카드 클릭 시 setActiveSysid로 활성.
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

                    // autopilot HEARTBEAT 수신 시각 기록 (per-vehicle 워치독용).
                    if (isAutopilot)
                        _lastSeen[fromSysid] = _uptime.elapsed();

                    // 모든 차량의 HEARTBEAT을 sysid 태그와 함께 전달 (VehicleManager가 분배).
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

                    // 모든 sysid → 카드 슬롯 (활성 필터 없음)
                    emit anyVehicleSysStatus(
                        static_cast<int>(_message.sysid),
                        static_cast<int>(static_cast<int8_t>(sys.battery_remaining)),
                        sys.voltage_battery / 1000.0f);

                    // 모든 차량의 상세 상태를 sysid 태그와 함께 전달 (VehicleManager가 분배)
                    {
                        MavlinkSysStatus out;
                        out.sysid            = _message.sysid;
                        out.voltageBattery   = sys.voltage_battery;
                        out.currentBattery   = sys.current_battery;
                        out.batteryRemaining = sys.battery_remaining;
                        out.dropRateComm     = sys.drop_rate_comm;  // SITL: 항상 0
                        out.errorsComm       = sys.errors_comm;
                        emit sysStatusReceived(out);
                    }
                    break;
                }

                case MAVLINK_MSG_ID_RADIO_STATUS: {
                    // RADIO_STATUS는 라디오 모뎀이 보냄 (sysid 0 또는 51). 활성 차량 필터 안 함.
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
    LOG_INFO("Send ARM_DISARM → target %d:%d : %s (force)",
             _activeSysid, _targetCompid, arm ? "ARM" : "DISARM");

    // param2 = 21196 (magic value, QGC의 "Force Arm" 동일):
    // pre-arm 체크를 우회. SITL/dev에서 GPS lock 없거나 EKF 미수렴이어도
    // ARM 가능하게 함. 실차량 운용 시 안전 정책 별도 고려 필요.
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(
        255, 0, &msg,                     // GCS sysid, compid
        _activeSysid, _targetCompid,      // target = HEARTBEAT에서 latch된 값
        MAV_CMD_COMPONENT_ARM_DISARM,
        0,                                // confirmation
        arm ? 1.0f : 0.0f,                // param1
        arm ? 21196.0f : 0.0f,            // param2: 21196 = force when arming
        0, 0, 0, 0, 0                     // param3~7 unused
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
                 _activeSysid, x, y, z, r, buttons);
    }

    mavlink_message_t msg;
    // buttons2/enabled_extensions/aux1~6/s/t: 사용하지 않으므로 0
    mavlink_msg_manual_control_pack(255, 0, &msg,
                                    _activeSysid,
                                    x, y, z, r,
                                    buttons,
                                    0, 0,                  // buttons2, enabled_extensions
                                    0, 0, 0, 0, 0, 0, 0, 0); // s, t, aux1~6
    _emitMessage(msg);
#else
    Q_UNUSED(x); Q_UNUSED(y); Q_UNUSED(z); Q_UNUSED(r); Q_UNUSED(buttons);
#endif
}


// ─────────────────────────────────────────────────────────────────────────────
// startHeartbeat() / stopHeartbeat()
// linkConnected/linkDisconnected 에 연결. GCS HEARTBEAT 타이머와 차량 watchdog를
// 함께 제어한다.
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


// per-vehicle 워치독 스윕: 마지막 HEARTBEAT가 kVehicleTimeoutMs 이상 끊긴 차량을
// 추적에서 제거하고 vehicleTimedOut(sysid)을 발신한다 (링크는 유지).
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
// 연결 해제 시 호출. 감지된 sysid 목록과 active sysid를 모두 초기화한다.
// 다음 연결의 첫 HEARTBEAT가 다시 latch 단계부터 진행.
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
// 사용자가 트리에서 다른 sysid를 클릭했을 때 호출. 0이면 active 해제.
// 감지 목록에 없는 sysid라도 설정 가능 (대기 상태로).
// ─────────────────────────────────────────────────────────────────────────────
void MavlinkManager::setActiveSysid(int sysid)
{
    const uint8_t s = static_cast<uint8_t>(sysid);
    if (s == _activeSysid) return;
    _activeSysid = s;
    if (s != 0)
        _targetCompid = _sysidCompid.value(s, 1);   // 알려진 compid 또는 ArduSub 기본 1
    LOG_INFO("Active sysid switched to %d (compid=%d)", _activeSysid, _targetCompid);
    emit activeSysidChanged(sysid);
}


// ─────────────────────────────────────────────────────────────────────────────
// sendSetMode()
// MAVLink SET_MODE 메시지로 ArduSub의 비행 모드를 변경한다.
// base_mode에 MAV_MODE_FLAG_CUSTOM_MODE_ENABLED 비트를 켜고
// custom_mode에 모드 번호 전달.
//
// ArduSub custom_mode 값:
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
