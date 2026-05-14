#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QQuickWidget>
#include "vehicle/VehicleState.h"
#include "ui/MapBridge.h"
#include "ui/HudWidget.h"
#include "ui/joystick/JoystickWidget.h"
#include "terrain/TerrainWidget.h"

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// GCS 메인 창. VehicleState의 변경 신호를 받아 각 레이블을 갱신하고
// 탭 전환 시 3D 지형 위젯에 현재 맵 중심을 전달한다.
//
// 탭 구성:
//   Status   — 배터리·자세·항법·GPS 수치 표시
//   OSM      — CartoDB dark_all 타일 맵 (QML + Qt Location)
//   Voyager  — CartoDB Voyager 타일 맵  (QML + Qt Location)
//   3D Terrain — Qt3D 기반 3D 지형 렌더러
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(VehicleState* state, QWidget* parent = nullptr);

public slots:
    void onBatteryChanged();
    void onAttitudeChanged();
    void onVfrHudChanged();
    void onGpsChanged();
    void onLinkConnected();
    void onLinkDisconnected();

private:
    void _setupUi();

    VehicleState* _state = nullptr;

    QLabel* _labelStatus   = nullptr;

    QLabel* _labelBattery  = nullptr;
    QLabel* _labelVoltage  = nullptr;
    QLabel* _labelCurrent  = nullptr;

    QLabel* _labelRoll     = nullptr;
    QLabel* _labelPitch    = nullptr;
    QLabel* _labelYaw      = nullptr;

    QLabel* _labelDepth    = nullptr;
    QLabel* _labelSpeed    = nullptr;
    QLabel* _labelHeading  = nullptr;
    QLabel* _labelThrottle = nullptr;

    QLabel* _labelSats     = nullptr;
    QLabel* _labelHdop     = nullptr;
    QLabel* _labelLatLon   = nullptr;

    MapBridge       _mapBridge;
    HudWidget*      _hudWidget        = nullptr;
    JoystickWidget* _joystickWidget   = nullptr;
    QQuickWidget*   _mapWidget        = nullptr;
    QQuickWidget*   _positronWidget   = nullptr;
    TerrainWidget*  _terrainWidget    = nullptr;

public:
    JoystickWidget* joystickWidget() const { return _joystickWidget; }
};
