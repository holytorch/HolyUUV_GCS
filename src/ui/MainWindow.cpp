#include "MainWindow.h"
#include "terrain/TerrainScene.h"
#include <QStatusBar>
#include <QQmlContext>

MainWindow::MainWindow(VehicleState* state, QWidget* parent)
    : QMainWindow(parent), _state(state)
{
    setWindowTitle("HolyUUV GCS");
    resize(1600, 900);
    statusBar()->hide();
    _setupUi();

    connect(_state, &VehicleState::gpsChanged, this, &MainWindow::onGpsChanged);
}


void MainWindow::_setupUi()
{
    _mapBridge.initCacheDir();

    _missionTerrainScene = new TerrainScene(this);
    _logFeed             = new LogFeed(_state, this);
    _commander           = new VehicleCommander(this);
    _connection          = new ConnectionBridge(this);

    QQuickWidget* missionWidget = new QQuickWidget(this);
    missionWidget->rootContext()->setContextProperty("bridge",               &_mapBridge);
    missionWidget->rootContext()->setContextProperty("missionTerrainScene",  _missionTerrainScene);
    missionWidget->rootContext()->setContextProperty("logFeed",              _logFeed);
    missionWidget->rootContext()->setContextProperty("vehicle",              _state);
    missionWidget->rootContext()->setContextProperty("commander",            _commander);
    missionWidget->rootContext()->setContextProperty("connection",           _connection);
    missionWidget->setSource(QUrl("qrc:/qml/MissionView.qml"));
    missionWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    setCentralWidget(missionWidget);
}


void MainWindow::onGpsChanged()
{
    _mapBridge.updatePosition(_state->latitude(), _state->longitude());
    if (_missionTerrainScene)
        _missionTerrainScene->updateVehiclePosition(
            _state->latitude(), _state->longitude());
}
