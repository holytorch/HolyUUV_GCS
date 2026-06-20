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
    setMinimumSize(1280, 960);   // 이 이하로는 축소 불가 (레이아웃 깨짐 방지)
    statusBar()->hide();
    _setupUi();

    // 활성 차량 전환 → vehicle 노출 + 맵 중심/청크 포커스 재바인딩
    connect(_vehicles, &VehicleManager::activeVehicleChanged,
            this, &MainWindow::_onActiveVehicleChanged);
    // 차량 추가/제거 → 3D 마커(sysid별) 연결/해제
    connect(_vehicles, &VehicleManager::vehicleAdded,
            this, &MainWindow::_onVehicleAdded);
    connect(_vehicles, &VehicleManager::vehicleRemoved,
            this, &MainWindow::_onVehicleRemoved);
    _onActiveVehicleChanged();   // 초기 상태 (보통 아직 차량 없음 → vehicle=null)

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
    // 다중로봇: 차량 리스트 모델 + 현재 활성 차량
    _qmlCtx->setContextProperty("vehicleManager",    _vehicles);
    _qmlCtx->setContextProperty("vehicle",           _vehicles->activeVehicle());
    mainWidget->setSource(QUrl("qrc:/qml/MainView.qml"));
    mainWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    setCentralWidget(mainWidget);
}


// 활성 차량 전환: 비파괴적 — 모든 차량 데이터·마커는 유지되고, 화면이 "보는/조종하는"
// 대상만 바뀐다. 여기선 (1) QML vehicle 노출, (2) 2D 맵 자동중심(활성),
// (3) 3D 청크/카메라 포커스를 활성 차량으로 전환한다. (3D 마커 자체는 차량별 연결이 담당)
void MainWindow::_onActiveVehicleChanged()
{
    // 이전 활성 차량의 시그널 연결 해제 (네트워크 아님 — 시그널/슬롯만)
    for (const auto& c : _activeVehicleConns)
        disconnect(c);
    _activeVehicleConns.clear();

    VehicleState* v = _vehicles->activeVehicle();

    // QML의 단일 'vehicle' 바인딩을 새 활성 차량으로 갱신 (컨트롤센터/마커 강조 공용)
    if (_qmlCtx)
        _qmlCtx->setContextProperty("vehicle", static_cast<QObject*>(v));

    // 3D 청크/카메라가 추종할 활성 차량 지정
    if (_mainTerrainScene)
        _mainTerrainScene->setActiveSysid(_vehicles->activeSysid());

    if (!v) return;

    // 활성 차량: 2D 맵 자동중심 + 청크 전방 우선 로딩용 heading
    _activeVehicleConns << connect(v, &VehicleState::gpsChanged,
                                   this, &MainWindow::onGpsChanged);
    _activeVehicleConns << connect(v, &VehicleState::vfrHudChanged,
                                   this, &MainWindow::onDepthChanged);
    onGpsChanged();
    onDepthChanged();
}


// 새 차량 → 그 차량의 위치/수심/yaw 시그널을 sysid별 3D 마커에 연결한다.
// (VehicleState가 제거되면 Qt가 연결을 자동 해제하므로 수동 추적 불필요)
void MainWindow::_onVehicleAdded(int sysid)
{
    VehicleState* v = _vehicles->vehicle(sysid);
    if (!v || !_mainTerrainScene) return;
    TerrainScene* ts = _mainTerrainScene;

    connect(v, &VehicleState::gpsChanged, this, [ts, v, sysid]() {
        ts->updateVehiclePosition(sysid, v->latitude(), v->longitude());
    });
    connect(v, &VehicleState::vfrHudChanged, this, [ts, v, sysid]() {
        const float d = -v->depth();              // 수면 아래 음수 → 양수 깊이(m)
        ts->updateVehicleDepth(sysid, d > 0.0f ? d : 0.0f);
    });
    connect(v, &VehicleState::attitudeChanged, this, [ts, v, sysid]() {
        // +90°: DAVE 시뮬 yaw 프레임 보정 (2D 마커와 동일)
        ts->setVehicleYaw(sysid, v->yaw() * 180.0 / M_PI + 90.0);
    });

    // 이미 들어와 있는 값으로 즉시 1회 갱신 (없으면 0,0 → 내부에서 무시)
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


// 활성 차량 2D 맵 자동중심 (MapBridge → QML 맵 panTo)
void MainWindow::onGpsChanged()
{
    VehicleState* v = _vehicles->activeVehicle();
    if (!v) return;
    _mapBridge.updatePosition(v->latitude(), v->longitude());
}


// 활성 차량 heading → 3D 청크 전방 우선 로딩 (수심·yaw 마커는 차량별 연결이 담당)
void MainWindow::onDepthChanged()
{
    VehicleState* v = _vehicles->activeVehicle();
    if (!v || !_mainTerrainScene) return;
    _mainTerrainScene->setVehicleHeading(v->heading());
}
