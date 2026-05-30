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

    _mainTerrainScene = new TerrainScene(this);
    _logFeed          = new LogFeed(_state, this);
    _commander        = new VehicleCommander(this);
    _connection       = new ConnectionBridge(this);

    QQuickWidget* mainWidget = new QQuickWidget(this);
    mainWidget->rootContext()->setContextProperty("bridge",            &_mapBridge);
    mainWidget->rootContext()->setContextProperty("mainTerrainScene",  _mainTerrainScene);
    mainWidget->rootContext()->setContextProperty("logFeed",           _logFeed);
    mainWidget->rootContext()->setContextProperty("vehicle",           _state);
    mainWidget->rootContext()->setContextProperty("commander",         _commander);
    mainWidget->rootContext()->setContextProperty("connection",        _connection);
    mainWidget->setSource(QUrl("qrc:/qml/MainView.qml"));
    mainWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    setCentralWidget(mainWidget);
}


void MainWindow::onGpsChanged()
{
    _mapBridge.updatePosition(_state->latitude(), _state->longitude());
    if (_mainTerrainScene)
        _mainTerrainScene->updateVehiclePosition(
            _state->latitude(), _state->longitude());
}
