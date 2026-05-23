#include "MainWindow.h"
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

    // ── 중앙 영역: 좌(HUD) + 중(지도) + 우(3D) ──────────────────
    QHBoxLayout* midLayout = new QHBoxLayout();
    midLayout->setSpacing(4);

    // MapBridge 캐시 디렉토리 초기화 (Mission탭 맵이 사용)
    _mapBridge.initCacheDir();

    // 좌측: HUD 계기판
    _hudWidget = new HudWidget(_state, root);
    _hudWidget->setFixedWidth(480);
    midLayout->addWidget(_hudWidget);

    // 중앙: (비어 있음 — 추후 다른 위젯 배치 예정)
    midLayout->addStretch(1);

    // 우측: 3D Terrain
    _terrainWidget = new TerrainWidget(root);
    _terrainWidget->setFixedWidth(440);
    midLayout->addWidget(_terrainWidget);

    rootLayout->addLayout(midLayout, 1);

    // ── 하단: 조이스틱 ───────────────────────────────────────────
    _joystickWidget = new JoystickWidget(root);
    _joystickWidget->setFixedHeight(600);
    rootLayout->addWidget(_joystickWidget);

    QQuickWidget* missionWidget = new QQuickWidget(this);
    missionWidget->rootContext()->setContextProperty("bridge", &_mapBridge);
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
    _terrainWidget->updateVehiclePosition(_state->latitude(), _state->longitude());
}
