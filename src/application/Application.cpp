#include "Application.h"
#include "comm/UdpLink.h"
#include "comm/SerialLink.h"
#include "comm/UsbBoardInfo.h"
#include <QSerialPortInfo>

Application::Application(QObject* parent) : QObject(parent)
{
    _tileServer.start();

    // 링크 상태 → MainWindow
    connect(&_linkManager, &LinkManager::linkConnected,
            &_mainWindow, &MainWindow::onLinkConnected);
    connect(&_linkManager, &LinkManager::linkDisconnected,
            &_mainWindow, &MainWindow::onLinkDisconnected);
    connect(&_linkManager, &LinkManager::linkError, [](const QString& msg) {
        qCritical("Link error: %s", qPrintable(msg));
    });

    // 수신 바이트 → MAVLink 파싱
    connect(&_linkManager, &LinkManager::bytesReceived,
            &_mavlinkManager, &MavlinkManager::parseBytes);

    // MAVLink → VehicleState
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

    // Serial 포트 스캔 → VID/PID로 MAVLink 보드 감지, 없으면 UDP
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
}
