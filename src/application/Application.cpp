#include "Application.h"
#include "comm/UdpLink.h"
#include "ui/VehicleCommander.h"
#include "ui/ConnectionBridge.h"
#include <QApplication>

// ─────────────────────────────────────────────────────────────────────────────
// Application()
// ─────────────────────────────────────────────────────────────────────────────
Application::Application(QObject* parent) : QObject(parent)
{
    // LogFeed logs the active vehicle's state transitions (ARMED/MODE/GPS/BATTERY).
    // When the active vehicle changes it is rebound to the new one (the rebind
    // makes LogFeed detach from the previous vehicle first).
    connect(&_vehicleManager, &VehicleManager::activeVehicleChanged, this, [this]() {
        _logFeed.bindVehicle(_vehicleManager.activeVehicle());
    });
}


// ─────────────────────────────────────────────────────────────────────────────
// initialize()
// Wires all subsystems together and starts the application.
//
// Inbound data flow:
//   ArduSub → UdpLink → MavlinkManager → VehicleState → QML
//   (packet)  (bytes)   (parse/route)    (state)
//
// Outbound data flow:
//   QML → VehicleCommander → MavlinkManager → UdpLink → ArduSub
//   (input) (emit signal)    (assemble packet) (send)
//
// Initialization order:
//   1. Start the TileServer (listen on 127.0.0.1:17777)
//   2. Wire LinkManager ↔ MainWindow (connect/disconnect status)
//   3. Build the LinkManager → MavlinkManager → VehicleState data pipeline
//   4. Show the MainWindow
// ─────────────────────────────────────────────────────────────────────────────
bool Application::initialize()
{
    if (!_tileServer.start()) {
        qCritical("Application: TileServer failed to start");
        return false;
    }

    // ILink → LinkManager → MavlinkManager data pipeline
    connect(&_linkManager, &LinkManager::linkError, [](const QString& msg) {
        qCritical("Link error: %s", qPrintable(msg));
    });
    connect(&_linkManager, &LinkManager::bytesReceived,
            &_mavlinkManager, &MavlinkManager::parseBytes);

    // MavlinkManager → VehicleManager update pipeline (routed per vehicle by sysid)
    connect(&_mavlinkManager, &MavlinkManager::sysStatusReceived,
            [this](const MavlinkSysStatus& s) {
        _vehicleManager.getOrCreate(s.sysid)->updateBattery(
            s.batteryRemaining, s.voltageBattery / 1000.0f,
            s.currentBattery, s.dropRateComm, s.errorsComm);
    });
    connect(&_mavlinkManager, &MavlinkManager::radioStatusReceived,
            [this](const MavlinkRadioStatus& r) {
        // RADIO_STATUS is link (modem) level — reflected only on the active vehicle
        if (auto* v = _vehicleManager.activeVehicle())
            v->updateRadioStatus(r.rssi, r.remRssi);
    });
    connect(&_mavlinkManager, &MavlinkManager::attitudeReceived,
            [this](const MavlinkAttitude& a) {
        _vehicleManager.getOrCreate(a.sysid)->updateAttitude(a.roll, a.pitch, a.yaw);
    });
    connect(&_mavlinkManager, &MavlinkManager::vfrHudReceived,
            [this](const MavlinkVfrHud& h) {
        _vehicleManager.getOrCreate(h.sysid)->updateVfrHud(
            h.groundspeed, h.altitude, h.heading, h.throttle);
    });
    connect(&_mavlinkManager, &MavlinkManager::globalPositionReceived,
            [this](const MavlinkGlobalPosition& p) {
        _vehicleManager.getOrCreate(p.sysid)->updateGlobalPosition(p.lat, p.lon);
    });
    connect(&_mavlinkManager, &MavlinkManager::gpsRawReceived,
            [this](const MavlinkGpsRaw& g) {
        _vehicleManager.getOrCreate(g.sysid)->updateGpsRaw(g.satCount, g.hdop);
    });
    connect(&_mavlinkManager, &MavlinkManager::heartbeatReceived,
            [this](const MavlinkHeartbeat& hb) {
        const bool armed = (hb.baseMode & 0x80) != 0;
        _vehicleManager.getOrCreate(hb.sysid)->updateHeartbeat(armed, hb.customMode);
    });

    // Active-vehicle authority = VehicleManager. When the active vehicle changes
    // (whether by user click or automatic re-selection after a timeout), the
    // command/heartbeat target (Mavlink) and the UI highlight (ConnBridge) follow.
    connect(&_vehicleManager, &VehicleManager::activeVehicleChanged, this,
            [this]() {
        const int sysid = _vehicleManager.activeSysid();
        _mavlinkManager.setActiveSysid(sysid);   // command target + compid resolution
        if (auto* conn = _mainWindow.connection())
            conn->setActiveSysidMirror(sysid);   // card highlight
    });

    // New sysid detected → pre-create the vehicle object + mirror into
    // ConnectionBridge (added to the QML tree)
    connect(&_mavlinkManager, &MavlinkManager::sysidDetected,
            [this](int sysid) {
        _vehicleManager.getOrCreate(sysid);
        if (auto* conn = _mainWindow.connection())
            conn->addDetectedSysid(sysid);
    });

    // SYS_STATUS from any sysid → ConnectionBridge slot (for the VEHICLES cards)
    if (auto* conn = _mainWindow.connection()) {
        connect(&_mavlinkManager, &MavlinkManager::anyVehicleSysStatus,
                conn, &ConnectionBridge::onAnyVehicleSysStatus);
    }

    // MAVLink transmit → LinkManager
    connect(&_mavlinkManager, &MavlinkManager::bytesToSend,
            [this](const QByteArray& data) { _linkManager.sendBytes(data); });

    // GCS HEARTBEAT timer + vehicle watchdog control
    connect(&_linkManager, &LinkManager::linkConnected,
            &_mavlinkManager, &MavlinkManager::startHeartbeat);
    connect(&_linkManager, &LinkManager::linkDisconnected,
            &_mavlinkManager, &MavlinkManager::stopHeartbeat);
    // A specific vehicle's HEARTBEAT drops out → remove only that vehicle (the link
    // stays up, other vehicles are unaffected)
    connect(&_mavlinkManager, &MavlinkManager::vehicleTimedOut,
            [this](int sysid) {
        _vehicleManager.removeVehicle(sysid);
        if (auto* conn = _mainWindow.connection())
            conn->removeDetectedSysid(sysid);
    });

    // QML control-center buttons / joystick → MAVLink transmit
    if (auto* cmd = _mainWindow.commander()) {
        connect(cmd, &VehicleCommander::armRequested,
                &_mavlinkManager, &MavlinkManager::sendArmDisarm);
        connect(cmd, &VehicleCommander::setModeRequested,
                &_mavlinkManager, &MavlinkManager::sendSetMode);
        connect(cmd, &VehicleCommander::manualControlRequested,
                &_mavlinkManager, &MavlinkManager::sendManualControl);
    }

    // QML connection bridge → LinkManager (Phase 1: single-link).
    // Connect request: any existing link is torn down automatically before a new
    // UdpLink is created.
    if (auto* conn = _mainWindow.connection()) {
        connect(conn, &ConnectionBridge::connectRequested,
                [this](const QString& host, quint16 port) {
            UdpConfig cfg;
            cfg.remoteHost = host;
            cfg.remotePort = port;
            cfg.localPort  = port;   // QGC-style: bind this port to receive every robot's push
            _linkManager.setLink(std::make_unique<UdpLink>(cfg));
        });
        connect(conn, &ConnectionBridge::disconnectRequested,
                [this]() { _linkManager.removeLink(); });

        // Reflect LinkManager state changes into the bridge (QML connected property)
        connect(&_linkManager, &LinkManager::linkConnected, conn,
                [conn]() { conn->setConnected(true); });
        connect(&_linkManager, &LinkManager::linkDisconnected, conn,
                [conn, this]() {
            conn->setConnected(false);
            conn->clearDetectedSysids();
            // On disconnect, reset the mavlink sysid latch + remove all vehicles
            _mavlinkManager.resetVehicleLatch();
            _vehicleManager.clear();
        });

        // QML clicks a sysid → request a change to VehicleManager (the active
        // authority). When VM emits activeVehicleChanged, the lambda above syncs the
        // Mavlink target / highlight.
        connect(conn, &ConnectionBridge::activeSysidChangeRequested,
                &_vehicleManager, &VehicleManager::setActiveSysid);
    }

    _mainWindow.show();

    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// run()
// Starts the Qt event loop. Blocks until the window is closed and returns the
// exit code.
// ─────────────────────────────────────────────────────────────────────────────
int Application::run()
{
    return QApplication::exec();
}


// ─────────────────────────────────────────────────────────────────────────────
// shutdown()
// Called after the event loop ends. Cleanup is currently automatic via the Qt
// parent-child relationships.
// ─────────────────────────────────────────────────────────────────────────────
void Application::shutdown()
{
    qInfo("Application: shutdown");
}
