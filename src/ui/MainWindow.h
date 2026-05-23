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
// 탭 구조의 GCS 메인 창.
//
// 탭:
//   Operations — HUD + 3D Terrain + Joystick
//   Mission    — QML 기반 미션 뷰 (OSM/Voyager 토글 맵 포함)
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(VehicleState* state, QWidget* parent = nullptr);

public slots:
    void onGpsChanged();

private:
    void _setupUi();

    VehicleState*   _state           = nullptr;
    MapBridge       _mapBridge;
    HudWidget*      _hudWidget       = nullptr;
    JoystickWidget* _joystickWidget  = nullptr;
    TerrainWidget*  _terrainWidget   = nullptr;

public:
    JoystickWidget* joystickWidget() const { return _joystickWidget; }
};
