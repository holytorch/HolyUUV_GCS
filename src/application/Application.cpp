#include "Application.h"
#include "comm/UdpLink.h"
// #include "comm/SerialLink.h"   // 실기기 대응 시 활성화
// #include "comm/UsbBoardInfo.h" // 실기기 대응 시 활성화
#include "ui/joystick/JoystickWidget.h"
#include <QApplication>
// #include <QSerialPortInfo>     // 실기기 대응 시 활성화

// ─────────────────────────────────────────────────────────────────────────────
// Application()
// ─────────────────────────────────────────────────────────────────────────────
Application::Application(QObject* parent) : QObject(parent) {}


// ─────────────────────────────────────────────────────────────────────────────
// initialize()
// 모든 하위 시스템을 연결하고 애플리케이션을 시작한다.
//
// 수신 데이터 흐름:
//   ArduSub → UdpLink → MavlinkManager → VehicleState → HudWidget → 화면
//   (패킷)    (바이트)   (파싱/분류)      (상태저장)     (렌더링)
//
// 송신 데이터 흐름:
//   조이스틱 → JoystickWidget → MavlinkManager → UdpLink → ArduSub
//   (입력)     (시그널 발신)    (패킷 조립)      (송신)
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



    // JoystickWidget → MavlinkManager → LinkManager 송신 파이프라인
    connect(_mainWindow.joystickWidget(), &JoystickWidget::joystickState,
            &_mavlinkManager, &MavlinkManager::sendManualControl);
    connect(&_mavlinkManager, &MavlinkManager::bytesToSend,
            [this](const QByteArray& data) { _linkManager.sendBytes(data); });
    connect(_mainWindow.joystickWidget(), &JoystickWidget::armRequested,
            &_mavlinkManager, &MavlinkManager::sendArmDisarm);
    connect(_mainWindow.joystickWidget(), &JoystickWidget::connectRequested,
            [this](const QString& host, quint16 port) {
        UdpConfig cfg;
        cfg.remoteHost = host;
        cfg.remotePort = port;
        cfg.localPort  = port;
        _linkManager.setLink(std::make_unique<UdpLink>(cfg));
    });
    connect(_mainWindow.joystickWidget(), &JoystickWidget::disconnectRequested,
            [this]() { _linkManager.removeLink(); });


    
    // LinkManager → JoystickWidget 연결 상태 표시
    connect(&_linkManager, &LinkManager::linkConnected,
            _mainWindow.joystickWidget(), &JoystickWidget::onLinkConnected);
    connect(&_linkManager, &LinkManager::linkDisconnected,
            _mainWindow.joystickWidget(), &JoystickWidget::onLinkDisconnected);




    // 시그널-슬롯 전부 연결 후에 창을 표시한다 (연결 상태 초기화 위해)
    _mainWindow.show();


    // 실기기 대응 코드 (GCS 개발 완료 후 차후 진행 예정)
    // QString mavlinkPort;
    // const UsbBoardInfo& boardInfo = UsbBoardInfo::instance();
    // for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
    //     if (boardInfo.isMavlinkBoard(info)) {
    //         mavlinkPort = info.systemLocation();
    //         qInfo("MAVLink board detected: %s (%s)",
    //               qPrintable(boardInfo.boardName(info)),
    //               qPrintable(mavlinkPort));
    //         break;
    //     }
    // }
    // if (!mavlinkPort.isEmpty()) {
    //     SerialConfig config;
    //     config.portName = mavlinkPort;
    //     config.baudRate = 57600;
    //     qInfo("Serial port found: %s", qPrintable(mavlinkPort));
    //     _linkManager.setLink(std::make_unique<SerialLink>(config));
    // } else {
    //     qInfo("Serial port 없음 → Joystick 탭에서 UDP 연결");
    // }

    
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
