#include "Application.h"
#include "comm/UdpLink.h"
#include "comm/SerialLink.h"
#include "comm/UsbBoardInfo.h"
#include <QApplication>
#include <QSerialPortInfo>

// ─────────────────────────────────────────────────────────────────────────────
// Application()
// ─────────────────────────────────────────────────────────────────────────────
Application::Application(QObject* parent) : QObject(parent) {}


// ─────────────────────────────────────────────────────────────────────────────
// initialize()
// 모든 하위 시스템을 연결하고 애플리케이션을 시작한다.
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

    connect(&_linkManager, &LinkManager::linkConnected,
            &_mainWindow, &MainWindow::onLinkConnected);
    connect(&_linkManager, &LinkManager::linkDisconnected,
            &_mainWindow, &MainWindow::onLinkDisconnected);
    connect(&_linkManager, &LinkManager::linkError, [](const QString& msg) {
        qCritical("Link error: %s", qPrintable(msg));
    });

    connect(&_linkManager, &LinkManager::bytesReceived,
            &_mavlinkManager, &MavlinkManager::parseBytes);

    connect(&_mavlinkManager, &MavlinkManager::sysStatusReceived,
            [this](const MavlinkSysStatus& s) {
        _vehicleState.updateBattery(s.batteryRemaining,
                                    s.voltageBattery / 1000.0f,
                                    s.currentBattery);
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

    _mainWindow.show();

    QString mavlinkPort;
    const UsbBoardInfo& boardInfo = UsbBoardInfo::instance();
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        if (boardInfo.isMavlinkBoard(info)) {
            mavlinkPort = info.systemLocation();
            qInfo("MAVLink board detected: %s (%s)",
                  qPrintable(boardInfo.boardName(info)),
                  qPrintable(mavlinkPort));
            break;
        }
    }

    if (!mavlinkPort.isEmpty()) {
        SerialConfig config;
        config.portName = mavlinkPort;
        config.baudRate = 57600;
        qInfo("Serial port found: %s", qPrintable(mavlinkPort));
        _linkManager.setLink(std::make_unique<SerialLink>(config));
    } else {
        UdpConfig config;
        config.localPort = 14550;
        qInfo("Serial port 없음 → UDP 14550");
        _linkManager.setLink(std::make_unique<UdpLink>(config));
    }

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
