#include "MainWindow.h"
#include "terrain/TerrainScene.h"
#include <QStatusBar>
#include <QQmlContext>
#include <QDebug>
#include <cmath>

MainWindow::MainWindow(VehicleState* state, LogFeed* logFeed, QWidget* parent)
    : QMainWindow(parent), _state(state), _logFeed(logFeed)
{
    setWindowTitle("HolyUUV GCS");
    resize(1600, 900);
    statusBar()->hide();
    _setupUi();

    connect(_state, &VehicleState::gpsChanged, this, &MainWindow::onGpsChanged);
    // VFR_HUD(depth)가 갱신될 때마다 3D 마커 수심 반영
    connect(_state, &VehicleState::vfrHudChanged, this, &MainWindow::onDepthChanged);
    // ATTITUDE yaw → 3D 마커 콘 방향 (뱃머리 방향 표시).
    // +90°: DAVE 시뮬 yaw 프레임 보정 (2D 마커와 동일)
    connect(_state, &VehicleState::attitudeChanged, this, [this]() {
        if (_mainTerrainScene)
            _mainTerrainScene->setVehicleYaw(_state->yaw() * 180.0 / M_PI + 90.0);
    });

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


void MainWindow::onDepthChanged()
{
    if (!_mainTerrainScene) return;
    // heading은 전방 우선 청크 로딩에 사용 (VFR_HUD에 함께 들어옴)
    _mainTerrainScene->setVehicleHeading(_state->heading());
    // vehicle.depth는 수면 아래일 때 음수 → 양수 깊이(m)로 변환해 전달
    const float d = -_state->depth();
    _mainTerrainScene->updateVehicleDepth(d > 0.0f ? d : 0.0f);
}
