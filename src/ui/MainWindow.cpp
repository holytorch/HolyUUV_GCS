#include "MainWindow.h"
#include "terrain/TerrainScene.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QTabWidget>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>

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
    QWidget*     root       = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(4);

    // ── 중앙 영역: 좌(HUD) ───────────────────────────────────────
    QHBoxLayout* midLayout = new QHBoxLayout();
    midLayout->setSpacing(4);

    // MapBridge 캐시 디렉토리 초기화 (Mission탭 맵이 사용)
    _mapBridge.initCacheDir();

    // 좌측: HUD 계기판
    _hudWidget = new HudWidget(_state, root);
    _hudWidget->setFixedWidth(480);
    midLayout->addWidget(_hudWidget);

    midLayout->addStretch(1);

    rootLayout->addLayout(midLayout, 1);

    // ── 하단: 조이스틱 ───────────────────────────────────────────
    _joystickWidget = new JoystickWidget(root);
    _joystickWidget->setFixedHeight(600);
    rootLayout->addWidget(_joystickWidget);

    // Mission 탭: QML이 모든 모드를 자체 렌더링.
    // 3D 모드일 때 QML의 Scene3D 컴포넌트가 missionTerrainScene의 entity 트리를 합성.
    _missionTerrainScene = new TerrainScene(this);

    // 로그 피드 (VehicleState 신호 구독해서 의미있는 이벤트만 누적)
    _logFeed = new LogFeed(_state, this);

    // QML → MAVLink 송신 브리지 (ARM, 모드 변경 등). Application이 시그널을 wire.
    _commander = new VehicleCommander(this);

    // QML → LinkManager 연결 브리지 (UDP connect/disconnect).
    _connection = new ConnectionBridge(this);

    QQuickWidget* missionWidget = new QQuickWidget(this);
    missionWidget->rootContext()->setContextProperty("bridge", &_mapBridge);
    missionWidget->rootContext()->setContextProperty("missionTerrainScene", _missionTerrainScene);
    missionWidget->rootContext()->setContextProperty("logFeed", _logFeed);
    missionWidget->rootContext()->setContextProperty("vehicle", _state);
    missionWidget->rootContext()->setContextProperty("commander", _commander);
    missionWidget->rootContext()->setContextProperty("connection", _connection);
    missionWidget->setSource(QUrl("qrc:/qml/MissionView.qml"));
    missionWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    QTabWidget* tabs = new QTabWidget(this);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 0; background: #0d1620; }"
        "QTabBar { background: #0a1018; }"
        "QTabBar::tab {"
        "  background: #0a1018; color: #aaaaaa;"
        "  padding: 8px 18px; border: none;"
        "  font-size: 12px; font-weight: 600;"
        "}"
        "QTabBar::tab:selected { background: #1a2530; color: #ffffff; }"
        "QTabBar::tab:hover:!selected { background: #14202c; color: #cccccc; }"
    );
    tabs->addTab(root, "Operations");
    tabs->addTab(missionWidget, "Mission");

    setCentralWidget(tabs);
}


void MainWindow::onGpsChanged()
{
    _mapBridge.updatePosition(_state->latitude(), _state->longitude());
    if (_missionTerrainScene)
        _missionTerrainScene->updateVehiclePosition(
            _state->latitude(), _state->longitude());
}
