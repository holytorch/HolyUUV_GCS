#pragma once

#include <QObject>
#include "comm/LinkManager.h"
#include "mavlink/MavlinkManager.h"
#include "vehicle/VehicleState.h"
#include "vehicle/VehicleManager.h"
#include "ui/LogFeed.h"
#include "ui/MainWindow.h"
#include "map/TileCache.h"
#include "map/TileServer.h"

// ─────────────────────────────────────────────────────────────────────────────
// Application
// The top-level controller of the GCS. It owns every subsystem (communication,
// MAVLink, vehicle state, tiles, UI) and wires them together with signals/slots.
//
// Execution flow:
//   main()
//   └─ Application::initialize()  — wire subsystems, detect links, show the window
//   └─ Application::run()         — enter the Qt event loop (blocking)
//   └─ Application::shutdown()    — teardown on exit
//
// Ownership:
//   Application
//   ├── LinkManager    — switches Serial / UDP links and receives bytes
//   ├── MavlinkManager — parses the byte stream into MAVLink messages
//   ├── VehicleState   — stores parsed telemetry and notifies on change
//   ├── TileCache      — SQLite tile cache (5 GB LRU)
//   ├── TileServer     — local HTTP tile proxy (127.0.0.1:17777)
//   └── MainWindow     — the main UI window
// ─────────────────────────────────────────────────────────────────────────────
class Application : public QObject {
    Q_OBJECT
public:
    // 'explicit' allows only the intended constructor (e.g. with a parent
    // QObject*) and prevents implicit conversions.
    // QObject: the parent; Application: the derived child.
    explicit Application(QObject* parent = nullptr);

    bool initialize();
    int  run();
    void shutdown();

private:
    // When Application is constructed, these members are constructed with it.
    // _logFeed is declared first → constructed first → installs its message
    // handler first, capturing the [init]/[exit] logs of every subsequent object
    // in the in-app feed as well.
    LogFeed        _logFeed;
    LinkManager    _linkManager;
    MavlinkManager _mavlinkManager;
    // Single source of truth for multi-robot — owns a VehicleState per sysid
    // (RAII via the QObject parent-child relationship).
    VehicleManager _vehicleManager;
    TileCache      _tileCache;
    TileServer     _tileServer{&_tileCache};
    MainWindow     _mainWindow{&_vehicleManager, &_logFeed};
};
