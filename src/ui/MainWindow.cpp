#include "MainWindow.h"
#include <QWidget>
#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow()
// UI를 구성하고 VehicleState 변경 신호를 각 슬롯에 연결한다.
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(VehicleState* state, QWidget* parent)
    : QMainWindow(parent), _state(state)
{
    setWindowTitle("HolyUUV GCS");
    resize(1100, 650);
    _setupUi();

    connect(_state, &VehicleState::batteryChanged,  this, &MainWindow::onBatteryChanged);
    connect(_state, &VehicleState::attitudeChanged, this, &MainWindow::onAttitudeChanged);
    connect(_state, &VehicleState::vfrHudChanged,   this, &MainWindow::onVfrHudChanged);
    connect(_state, &VehicleState::gpsChanged,      this, &MainWindow::onGpsChanged);
}


// ─────────────────────────────────────────────────────────────────────────────
// _setupUi()
// Status / OSM / Voyager / 3D Terrain 네 탭으로 구성된 UI를 생성한다.
// 3D Terrain 탭으로 전환 시 OSM 맵 중심 좌표를 TerrainWidget에 전달한다.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::_setupUi()
{
    QTabWidget* tabs = new QTabWidget(this);

    // ── Status 탭 ────────────────────────────────────────────
    QWidget*     statusTab = new QWidget();
    QVBoxLayout* root      = new QVBoxLayout(statusTab);

    _labelStatus = new QLabel("● Disconnected");
    _labelStatus->setStyleSheet("color: red; font-weight: bold;");
    root->addWidget(_labelStatus);

    QGroupBox*   battGroup  = new QGroupBox("Battery");
    QGridLayout* battLayout = new QGridLayout(battGroup);
    battLayout->addWidget(new QLabel("Remaining:"), 0, 0);
    battLayout->addWidget(_labelBattery  = new QLabel("---%"),    0, 1);
    battLayout->addWidget(new QLabel("Voltage:"),   0, 2);
    battLayout->addWidget(_labelVoltage  = new QLabel("--.- V"),  0, 3);
    battLayout->addWidget(new QLabel("Current:"),   0, 4);
    battLayout->addWidget(_labelCurrent  = new QLabel("--.- A"),  0, 5);
    root->addWidget(battGroup);

    QGroupBox*   attGroup  = new QGroupBox("Attitude");
    QGridLayout* attLayout = new QGridLayout(attGroup);
    attLayout->addWidget(new QLabel("Roll:"),  0, 0);
    attLayout->addWidget(_labelRoll  = new QLabel("--.-°"), 0, 1);
    attLayout->addWidget(new QLabel("Pitch:"), 0, 2);
    attLayout->addWidget(_labelPitch = new QLabel("--.-°"), 0, 3);
    attLayout->addWidget(new QLabel("Yaw:"),   0, 4);
    attLayout->addWidget(_labelYaw   = new QLabel("--.-°"), 0, 5);
    root->addWidget(attGroup);

    QGroupBox*   navGroup  = new QGroupBox("Navigation");
    QGridLayout* navLayout = new QGridLayout(navGroup);
    navLayout->addWidget(new QLabel("Depth:"),    0, 0);
    navLayout->addWidget(_labelDepth    = new QLabel("--.- m"),   0, 1);
    navLayout->addWidget(new QLabel("Speed:"),    0, 2);
    navLayout->addWidget(_labelSpeed    = new QLabel("--.- m/s"), 0, 3);
    navLayout->addWidget(new QLabel("Heading:"),  0, 4);
    navLayout->addWidget(_labelHeading  = new QLabel("---°"),     0, 5);
    navLayout->addWidget(new QLabel("Throttle:"), 1, 0);
    navLayout->addWidget(_labelThrottle = new QLabel("---%"),     1, 1);
    root->addWidget(navGroup);

    QGroupBox*   gpsGroup  = new QGroupBox("GPS");
    QGridLayout* gpsLayout = new QGridLayout(gpsGroup);
    gpsLayout->addWidget(new QLabel("Satellites:"), 0, 0);
    gpsLayout->addWidget(_labelSats   = new QLabel("--"),        0, 1);
    gpsLayout->addWidget(new QLabel("HDOP:"),       0, 2);
    gpsLayout->addWidget(_labelHdop   = new QLabel("--.-"),      0, 3);
    gpsLayout->addWidget(new QLabel("Position:"),   1, 0);
    gpsLayout->addWidget(_labelLatLon = new QLabel("---, ---"),  1, 1, 1, 3);
    root->addWidget(gpsGroup);

    root->addStretch();
    tabs->addTab(statusTab, "Status");

    // ── OSM 맵 탭 ────────────────────────────────────────────
    _mapBridge.initCacheDir();

    _mapWidget = new QQuickWidget();
    _mapWidget->rootContext()->setContextProperty("bridge", &_mapBridge);
    _mapWidget->setSource(QUrl("qrc:/qml/MapView.qml"));
    _mapWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    tabs->addTab(_mapWidget, "OSM");

    // ── Voyager 맵 탭 ─────────────────────────────────────────
    _positronWidget = new QQuickWidget();
    _positronWidget->rootContext()->setContextProperty("bridge", &_mapBridge);
    _positronWidget->setSource(QUrl("qrc:/qml/VoyagerView.qml"));
    _positronWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    tabs->addTab(_positronWidget, "Voyager");

    // ── 3D Terrain 탭 ─────────────────────────────────────────
    _terrainWidget = new TerrainWidget();
    tabs->addTab(_terrainWidget, "3D Terrain");

    // 3D 탭 전환 시 OSM 맵 현재 중심으로 지형 자동 로드
    connect(tabs, &QTabWidget::currentChanged, [this, tabs](int) {
        if (tabs->currentWidget() == _terrainWidget)
            _terrainWidget->loadTile(_mapBridge.mapCenterLat(), _mapBridge.mapCenterLon(),
                                     _terrainWidget->currentZoom());
    });

    setCentralWidget(tabs);
}


// ─────────────────────────────────────────────────────────────────────────────
// onBatteryChanged()
// VehicleState::batteryChanged 신호에 연결된 슬롯.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onBatteryChanged()
{
    _labelBattery->setText(QString("%1%").arg(_state->batteryRemaining()));
    _labelVoltage->setText(QString("%1 V").arg(_state->voltage(), 0, 'f', 1));
    _labelCurrent->setText(QString("%1 A").arg(_state->current() / 100.0f, 0, 'f', 1));
}


// ─────────────────────────────────────────────────────────────────────────────
// onAttitudeChanged()
// VehicleState::attitudeChanged 신호에 연결된 슬롯. rad → deg 변환 후 표시.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onAttitudeChanged()
{
    const float toDeg = 180.0f / 3.14159265f;
    _labelRoll ->setText(QString("%1°").arg(_state->roll()  * toDeg, 0, 'f', 1));
    _labelPitch->setText(QString("%1°").arg(_state->pitch() * toDeg, 0, 'f', 1));
    _labelYaw  ->setText(QString("%1°").arg(_state->yaw()   * toDeg, 0, 'f', 1));
}


// ─────────────────────────────────────────────────────────────────────────────
// onVfrHudChanged()
// VehicleState::vfrHudChanged 신호에 연결된 슬롯.
// depth는 VFR_HUD altitude의 부호 반전값(수심이 양수로 표시되도록).
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onVfrHudChanged()
{
    _labelDepth   ->setText(QString("%1 m").arg(-_state->depth(), 0, 'f', 2));
    _labelSpeed   ->setText(QString("%1 m/s").arg(_state->groundspeed(), 0, 'f', 1));
    _labelHeading ->setText(QString("%1°").arg(_state->heading(), 0, 'f', 0));
    _labelThrottle->setText(QString("%1%").arg(_state->throttle()));
}


// ─────────────────────────────────────────────────────────────────────────────
// onGpsChanged()
// VehicleState::gpsChanged 신호에 연결된 슬롯.
// GPS 좌표를 MapBridge와 TerrainWidget에도 전달한다.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onGpsChanged()
{
    _labelSats  ->setText(QString::number(_state->gpsSatCount()));
    _labelHdop  ->setText(QString("%1").arg(_state->gpsHdop(), 0, 'f', 1));
    _labelLatLon->setText(QString("%1, %2")
        .arg(_state->latitude(),  0, 'f', 6)
        .arg(_state->longitude(), 0, 'f', 6));

    _mapBridge.updatePosition(_state->latitude(), _state->longitude());
    _terrainWidget->updateVehiclePosition(_state->latitude(), _state->longitude());
}


// ─────────────────────────────────────────────────────────────────────────────
// onLinkConnected() / onLinkDisconnected()
// LinkManager 상태 변경 시 상태 레이블 색상과 텍스트를 갱신한다.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onLinkConnected()
{
    _labelStatus->setText("● Connected");
    _labelStatus->setStyleSheet("color: green; font-weight: bold;");
}

void MainWindow::onLinkDisconnected()
{
    _labelStatus->setText("● Disconnected");
    _labelStatus->setStyleSheet("color: red; font-weight: bold;");
}
