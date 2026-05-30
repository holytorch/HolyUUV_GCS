#include "Application.h"
#include "comm/UdpLink.h"
#include "ui/VehicleCommander.h"
#include "ui/ConnectionBridge.h"
#include <QApplication>

// ─────────────────────────────────────────────────────────────────────────────
// Application()
// ─────────────────────────────────────────────────────────────────────────────
Application::Application(QObject* parent) : QObject(parent) {}


// ─────────────────────────────────────────────────────────────────────────────
// initialize()
// 모든 하위 시스템을 연결하고 애플리케이션을 시작한다.
//
// 수신 데이터 흐름:
//   ArduSub → UdpLink → MavlinkManager → VehicleState → QML
//   (패킷)    (바이트)   (파싱/분류)      (상태저장)
//
// 송신 데이터 흐름:
//   QML → VehicleCommander → MavlinkManager → UdpLink → ArduSub
//   (입력)  (시그널 발신)     (패킷 조립)      (송신)
//
// 초기화 순서:
//   1. TileServer 시작 (127.0.0.1:17777 리슨)
//   2. LinkManager ↔ MainWindow 연결 (접속·해제 상태 표시)
//   3. LinkManager → MavlinkManager → VehicleState 데이터 파이프라인 구성
//   4. MainWindow 표시
//   5. USB 시리얼 포트 스캔 → 감지되면 SerialLink, 없으면 UdpLink(14550) 사용
// ─────────────────────────────────────────────────────────────────────────────
bool Application::initialize()
{
    if (!_tileServer.start()) {
        qCritical("Application: TileServer 시작 실패");
        return false;
    }

    // ILink → LinkManager → MavlinkManager 데이터 파이프라인
    connect(&_linkManager, &LinkManager::linkError, [](const QString& msg) {
        qCritical("Link error: %s", qPrintable(msg));
    });
    connect(&_linkManager, &LinkManager::bytesReceived,
            &_mavlinkManager, &MavlinkManager::parseBytes);

    // MavlinkManager → VehicleState 업데이트 파이프라인
    connect(&_mavlinkManager, &MavlinkManager::sysStatusReceived,
            [this](const MavlinkSysStatus& s) {
        _vehicleState.updateBattery(s.batteryRemaining,
                                    s.voltageBattery / 1000.0f,
                                    s.currentBattery,
                                    s.dropRateComm,
                                    s.errorsComm);
    });
    connect(&_mavlinkManager, &MavlinkManager::radioStatusReceived,
            [this](const MavlinkRadioStatus& r) {
        _vehicleState.updateRadioStatus(r.rssi, r.remRssi);
    });
    connect(&_mavlinkManager, &MavlinkManager::attitudeReceived,
            [this](const MavlinkAttitude& a) {
        _vehicleState.updateAttitude(a.roll, a.pitch, a.yaw);
    });
    connect(&_mavlinkManager, &MavlinkManager::vfrHudReceived,
            [this](const MavlinkVfrHud& h) {
        _vehicleState.updateVfrHud(h.groundspeed, h.altitude, h.heading, h.throttle);
    });
    connect(&_mavlinkManager, &MavlinkManager::globalPositionReceived,
            [this](const MavlinkGlobalPosition& p) {
        _vehicleState.updateGlobalPosition(p.lat, p.lon);
    });
    connect(&_mavlinkManager, &MavlinkManager::gpsRawReceived,
            [this](const MavlinkGpsRaw& g) {
        _vehicleState.updateGpsRaw(g.satCount, g.hdop);
    });
    connect(&_mavlinkManager, &MavlinkManager::heartbeatReceived,
            [this](const MavlinkHeartbeat& hb) {
        const bool armed = (hb.baseMode & 0x80) != 0;
        _vehicleState.updateHeartbeat(armed, hb.customMode);
    });

    // 활성 sysid 변경 → VehicleState에 sysid 반영 + telemetry 리셋 (stale 표시 방지)
    connect(&_mavlinkManager, &MavlinkManager::activeSysidChanged,
            [this](int sysid) {
        _vehicleState.resetTelemetry();
        _vehicleState.setSysid(sysid);
        if (auto* conn = _mainWindow.connection())
            conn->setActiveSysidMirror(sysid);
    });

    // 새 sysid 감지 → ConnectionBridge 미러 (QML 트리에 추가)
    connect(&_mavlinkManager, &MavlinkManager::sysidDetected,
            [this](int sysid) {
        if (auto* conn = _mainWindow.connection())
            conn->addDetectedSysid(sysid);
    });

    // 모든 sysid의 SYS_STATUS → ConnectionBridge 슬롯 (VEHICLES 카드 표시용)
    if (auto* conn = _mainWindow.connection()) {
        connect(&_mavlinkManager, &MavlinkManager::anyVehicleSysStatus,
                conn, &ConnectionBridge::onAnyVehicleSysStatus);
    }

    // MAVLink 송신 → LinkManager
    connect(&_mavlinkManager, &MavlinkManager::bytesToSend,
            [this](const QByteArray& data) { _linkManager.sendBytes(data); });

    // GCS HEARTBEAT 타이머 + 차량 watchdog 제어
    connect(&_linkManager, &LinkManager::linkConnected,
            &_mavlinkManager, &MavlinkManager::startHeartbeat);
    connect(&_linkManager, &LinkManager::linkDisconnected,
            &_mavlinkManager, &MavlinkManager::stopHeartbeat);
    // 차량 5초 무응답 → 자동 disconnect
    connect(&_mavlinkManager, &MavlinkManager::vehicleTimedOut,
            [this]() { _linkManager.removeLink(); });

    // QML 컨트롤센터 버튼/조이스틱 → MAVLink 송신
    if (auto* cmd = _mainWindow.commander()) {
        connect(cmd, &VehicleCommander::armRequested,
                &_mavlinkManager, &MavlinkManager::sendArmDisarm);
        connect(cmd, &VehicleCommander::setModeRequested,
                &_mavlinkManager, &MavlinkManager::sendSetMode);
        connect(cmd, &VehicleCommander::manualControlRequested,
                &_mavlinkManager, &MavlinkManager::sendManualControl);
    }

    // QML connection 브리지 → LinkManager (Phase 1: single-link).
    // Connect 요청: 기존 링크가 있으면 자동 해제 후 새 UdpLink 생성.
    if (auto* conn = _mainWindow.connection()) {
        connect(conn, &ConnectionBridge::connectRequested,
                [this](const QString& host, quint16 port) {
            UdpConfig cfg;
            cfg.remoteHost = host;
            cfg.remotePort = port;
            cfg.localPort  = 0;   // OS가 빈 포트 자동 할당
            _linkManager.setLink(std::make_unique<UdpLink>(cfg));
        });
        connect(conn, &ConnectionBridge::disconnectRequested,
                [this]() { _linkManager.removeLink(); });

        // LinkManager 상태 변화를 브리지에 반영 (QML connected 프로퍼티)
        connect(&_linkManager, &LinkManager::linkConnected, conn,
                [conn]() { conn->setConnected(true); });
        connect(&_linkManager, &LinkManager::linkDisconnected, conn,
                [conn, this]() {
            conn->setConnected(false);
            conn->clearDetectedSysids();
            // 연결 해제 시 mavlink sysid latch + vehicle telemetry 초기화
            _mavlinkManager.resetVehicleLatch();
            _vehicleState.resetTelemetry();
            _vehicleState.setSysid(0);
        });

        // QML이 sysid 클릭 → MavlinkManager에 active 변경 요청
        connect(conn, &ConnectionBridge::activeSysidChangeRequested,
                &_mavlinkManager, &MavlinkManager::setActiveSysid);
    }

    _mainWindow.show();

    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// run()
// Qt 이벤트 루프를 시작한다. 창이 닫힐 때까지 블로킹되고 종료 코드를 반환한다.
// ─────────────────────────────────────────────────────────────────────────────
int Application::run()
{
    return QApplication::exec();
}


// ─────────────────────────────────────────────────────────────────────────────
// shutdown()
// 이벤트 루프 종료 후 호출된다. 현재는 Qt 부모 관계로 자동 정리된다.
// ─────────────────────────────────────────────────────────────────────────────
void Application::shutdown()
{
    qInfo("Application: shutdown");
}
