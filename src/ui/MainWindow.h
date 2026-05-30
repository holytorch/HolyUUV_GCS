#pragma once

#include <QMainWindow>
#include <QQuickWidget>
#include "vehicle/VehicleState.h"
#include "ui/MapBridge.h"
#include "ui/LogFeed.h"
#include "ui/VehicleCommander.h"
#include "ui/ConnectionBridge.h"

class TerrainScene;

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// MissionView.qml 단독 창 (Operations 탭 제거됨).
// QML이 맵/3D/로그/컨트롤센터/조이스틱 모든 UI를 자체 렌더링.
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(VehicleState* state, QWidget* parent = nullptr);

public slots:
    void onGpsChanged();

private:
    void _setupUi();

    VehicleState*     _state               = nullptr;
    MapBridge         _mapBridge;
    TerrainScene*     _missionTerrainScene = nullptr;
    LogFeed*          _logFeed             = nullptr;
    VehicleCommander* _commander           = nullptr;
    ConnectionBridge* _connection          = nullptr;

public:
    VehicleCommander* commander()  const { return _commander; }
    ConnectionBridge* connection() const { return _connection; }
};
