#pragma once

#include <QMainWindow>
#include <QQuickWidget>
#include <QList>
#include "vehicle/VehicleState.h"
#include "vehicle/VehicleManager.h"
#include "ui/MapBridge.h"
#include "ui/LogFeed.h"
#include "ui/VehicleCommander.h"
#include "ui/ConnectionBridge.h"

class TerrainScene;
class QQmlContext;

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
// MainView.qml 단독 창.
// QML이 맵/3D/로그/컨트롤센터/조이스틱 모든 UI를 자체 렌더링.
//
// 다중로봇: VehicleManager를 받아 QML에 vehicles 모델(vehicleManager)과
// 현재 활성 차량(vehicle)을 노출한다. 활성 차량이 바뀌면 vehicle 컨텍스트
// 프로퍼티를 다시 가리키고, 맵/3D 갱신 시그널도 활성 차량으로 재바인딩한다.
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(VehicleManager* vehicles, LogFeed* logFeed, QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    void onGpsChanged();    // 활성 차량 위치 → MapBridge / TerrainScene
    void onDepthChanged();  // 활성 차량 수심/heading → TerrainScene

private:
    void _setupUi();
    void _onActiveVehicleChanged();   // 활성 차량 전환 시 vehicle 노출 + 맵중심/청크포커스 재바인딩
    void _onVehicleAdded(int sysid);  // 새 차량 → 그 차량 시그널을 3D 마커(sysid별)에 연결
    void _onVehicleRemoved(int sysid);// 차량 제거 → 3D 마커 제거

    VehicleManager*   _vehicles          = nullptr;
    QQmlContext*      _qmlCtx            = nullptr;
    QList<QMetaObject::Connection> _activeVehicleConns;   // 활성 차량 시그널 연결 (전환 시 해제)

    MapBridge         _mapBridge;
    TerrainScene*     _mainTerrainScene  = nullptr;
    LogFeed*          _logFeed             = nullptr;
    VehicleCommander* _commander           = nullptr;
    ConnectionBridge* _connection          = nullptr;

public:
    VehicleCommander* commander()  const { return _commander; }
    ConnectionBridge* connection() const { return _connection; }
};
