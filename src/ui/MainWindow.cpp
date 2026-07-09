#include "MainWindow.h"
#include "terrain/TerrainScene.h"
#include <QStatusBar>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDebug>
#include <cmath>

MainWindow::MainWindow(VehicleManager* vehicles, LogFeed* logFeed, QWidget* parent)
    : QMainWindow(parent), _vehicles(vehicles), _logFeed(logFeed)
{
    setWindowTitle("HolyUUV GCS");
    resize(1600, 900);
    setMinimumSize(1280, 960);   // cannot be shrunk below this (prevents layout breakage)
    statusBar()->hide();
    _setupUi();

    // Active-vehicle switch → expose 'vehicle' + rebind map center / chunk focus
    connect(_vehicles, &VehicleManager::activeVehicleChanged,
            this, &MainWindow::_onActiveVehicleChanged);
    // Vehicle added/removed → connect/disconnect the per-sysid 3D marker
    connect(_vehicles, &VehicleManager::vehicleAdded,
            this, &MainWindow::_onVehicleAdded);
    connect(_vehicles, &VehicleManager::vehicleRemoved,
            this, &MainWindow::_onVehicleRemoved);
    _onActiveVehicleChanged();   // initial state (usually no vehicle yet → vehicle=null)

    qInfo("[init] MainWindow");
}


MainWindow::~MainWindow()
{
    qInfo("[exit] MainWindow");
}


void MainWindow::_setupUi()
{
    _mapBridge.initCacheDir();

    _mainTerrainScene = new TerrainScene(this);
    _commander        = new VehicleCommander(this);
    _connection       = new ConnectionBridge(this);

    QQuickWidget* mainWidget = new QQuickWidget(this);
    _qmlCtx = mainWidget->rootContext();
    _qmlCtx->setContextProperty("bridge",            &_mapBridge);
    _qmlCtx->setContextProperty("mainTerrainScene",  _mainTerrainScene);
    _qmlCtx->setContextProperty("logFeed",           _logFeed);
    _qmlCtx->setContextProperty("commander",         _commander);
    _qmlCtx->setContextProperty("connection",        _connection);
    // Multi-robot: the vehicle list model + the currently active vehicle
    _qmlCtx->setContextProperty("vehicleManager",    _vehicles);
    _qmlCtx->setContextProperty("vehicle",           _vehicles->activeVehicle());
    mainWidget->setSource(QUrl("qrc:/qml/MainView.qml"));
    mainWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    setCentralWidget(mainWidget);
}


// Active-vehicle switch: non-destructive — all vehicle data and markers are kept,
// and only the target being "viewed/controlled" changes. Here it (1) exposes the
// QML 'vehicle', (2) auto-centers the 2D map on the active vehicle, and (3) moves
// the 3D chunk/camera focus to it. (The 3D markers themselves are handled by the
// per-vehicle connections.)
void MainWindow::_onActiveVehicleChanged()
{
    // Disconnect the previous active vehicle's signals (signal/slot only — nothing network-related)
    for (const auto& c : _activeVehicleConns)
        disconnect(c);
    _activeVehicleConns.clear();

    VehicleState* v = _vehicles->activeVehicle();

    // Update QML's single 'vehicle' binding to the new active vehicle (shared by the
    // control center / marker highlight)
    if (_qmlCtx)
        _qmlCtx->setContextProperty("vehicle", static_cast<QObject*>(v));

    // Set which active vehicle the 3D chunks/camera follow
    if (_mainTerrainScene)
        _mainTerrainScene->setActiveSysid(_vehicles->activeSysid());

    if (!v) return;

    // Active vehicle: auto-center the 2D map + heading for forward-priority chunk loading
    _activeVehicleConns << connect(v, &VehicleState::gpsChanged,
                                   this, &MainWindow::onGpsChanged);
    _activeVehicleConns << connect(v, &VehicleState::vfrHudChanged,
                                   this, &MainWindow::onDepthChanged);
    onGpsChanged();
    onDepthChanged();
}


// New vehicle → connect its position/depth/yaw signals to the per-sysid 3D marker.
// (When the VehicleState is removed, Qt drops the connections automatically, so no
// manual tracking is needed.)
void MainWindow::_onVehicleAdded(int sysid)
{
    VehicleState* v = _vehicles->vehicle(sysid);
    if (!v || !_mainTerrainScene) return;
    TerrainScene* ts = _mainTerrainScene;

    connect(v, &VehicleState::gpsChanged, this, [ts, v, sysid]() {
        ts->updateVehiclePosition(sysid, v->latitude(), v->longitude());
    });
    connect(v, &VehicleState::vfrHudChanged, this, [ts, v, sysid]() {
        const float d = -v->depth();              // below-surface negative → positive depth (m)
        ts->updateVehicleDepth(sysid, d > 0.0f ? d : 0.0f);
    });
    connect(v, &VehicleState::attitudeChanged, this, [ts, v, sysid]() {
        // +90°: correction for the DAVE simulator's yaw frame (same as the 2D marker)
        ts->setVehicleYaw(sysid, v->yaw() * 180.0 / M_PI + 90.0);
    });

    // Update once immediately with the values already received (if none, 0,0 → ignored internally)
    ts->updateVehiclePosition(sysid, v->latitude(), v->longitude());
    const float d = -v->depth();
    ts->updateVehicleDepth(sysid, d > 0.0f ? d : 0.0f);
    ts->setVehicleYaw(sysid, v->yaw() * 180.0 / M_PI + 90.0);
}


void MainWindow::_onVehicleRemoved(int sysid)
{
    if (_mainTerrainScene)
        _mainTerrainScene->removeVehicle(sysid);
}


// Auto-center the 2D map on the active vehicle (MapBridge → QML map panTo)
void MainWindow::onGpsChanged()
{
    VehicleState* v = _vehicles->activeVehicle();
    if (!v) return;
    _mapBridge.updatePosition(v->latitude(), v->longitude());
}


// Active vehicle heading → forward-priority 3D chunk loading (depth/yaw markers are
// handled by the per-vehicle connections)
void MainWindow::onDepthChanged()
{
    VehicleState* v = _vehicles->activeVehicle();
    if (!v || !_mainTerrainScene) return;
    _mainTerrainScene->setVehicleHeading(v->heading());
}
