#pragma once

#include <QMainWindow>
#include <QQuickWidget>
#include <QList>
#include "vehicle/VehicleState.h"
#include "vehicle/VehicleManager.h"
#include "ui/MapBridge.h"
#include "ui/LogFeed.h"
#include "ui/VehicleCommander.h"
#include "ui/ConnectionBridge.h"

class TerrainScene;
class QQmlContext;

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// A single window hosting MainView.qml.
// QML renders the entire UI itself — map / 3D / log / control center / joystick.
//
// Multi-robot: receives the VehicleManager and exposes to QML both the vehicles
// model (vehicleManager) and the currently active vehicle (vehicle). When the
// active vehicle changes, the 'vehicle' context property is re-pointed and the
// map/3D update signals are rebound to the active vehicle.
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(VehicleManager* vehicles, LogFeed* logFeed, QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    void onGpsChanged();    // active vehicle position → MapBridge / TerrainScene
    void onDepthChanged();  // active vehicle depth/heading → TerrainScene

private:
    void _setupUi();
    void _onActiveVehicleChanged();   // on active-vehicle switch: expose 'vehicle' + rebind map center / chunk focus
    void _onVehicleAdded(int sysid);  // new vehicle → connect its signals to the per-sysid 3D marker
    void _onVehicleRemoved(int sysid);// vehicle removed → remove its 3D marker

    VehicleManager*   _vehicles          = nullptr;
    QQmlContext*      _qmlCtx            = nullptr;
    QList<QMetaObject::Connection> _activeVehicleConns;   // active-vehicle signal connections (dropped on switch)

    MapBridge         _mapBridge;
    TerrainScene*     _mainTerrainScene  = nullptr;
    LogFeed*          _logFeed             = nullptr;
    VehicleCommander* _commander           = nullptr;
    ConnectionBridge* _connection          = nullptr;

public:
    VehicleCommander* commander()  const { return _commander; }
    ConnectionBridge* connection() const { return _connection; }
};
