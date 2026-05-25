#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QQuickWidget>
#include "vehicle/VehicleState.h"
#include "ui/MapBridge.h"
#include "ui/HudWidget.h"
#include "ui/LogFeed.h"
#include "ui/VehicleCommander.h"
#include "ui/ConnectionBridge.h"
#include "ui/joystick/JoystickWidget.h"

class TerrainScene;

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// 탭 구조의 GCS 메인 창.
//
// 탭:
//   Operations — HUD + 3D Terrain(QWidget) + Joystick
//   Mission    — QML 미션 뷰. 맵 모드 "3d" 시 Loader가 Scene3D 컴포넌트를 띄우고
//                missionTerrainScene(C++ 인스턴스)을 attach하여 Qt3D 씬을 합성.
//                QML이 자체적으로 렌더링하므로 카드/로그/조이스틱 모두 위에 보임.
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(VehicleState* state, QWidget* parent = nullptr);

public slots:
    void onGpsChanged();

private:
    void _setupUi();

    VehicleState*   _state                 = nullptr;
    MapBridge       _mapBridge;
    HudWidget*      _hudWidget             = nullptr;
    JoystickWidget* _joystickWidget        = nullptr;
    TerrainScene*   _missionTerrainScene   = nullptr;
    LogFeed*        _logFeed               = nullptr;
    VehicleCommander* _commander           = nullptr;
    ConnectionBridge* _connection          = nullptr;

public:
    JoystickWidget*   joystickWidget() const { return _joystickWidget; }
    VehicleCommander* commander()       const { return _commander; }
    ConnectionBridge* connection()      const { return _connection; }
};
