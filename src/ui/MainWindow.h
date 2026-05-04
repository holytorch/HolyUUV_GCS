#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QQuickWidget>
#include "vehicle/VehicleState.h"
#include "ui/MapBridge.h"
#include "terrain/TerrainWidget.h"

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

    // 상태바
    QLabel* _labelStatus     = nullptr;

    // 배터리
    QLabel* _labelBattery    = nullptr;
    QLabel* _labelVoltage    = nullptr;
    QLabel* _labelCurrent    = nullptr;

    // 자세
    QLabel* _labelRoll       = nullptr;
    QLabel* _labelPitch      = nullptr;
    QLabel* _labelYaw        = nullptr;

    // 위치/이동
    QLabel* _labelDepth      = nullptr;
    QLabel* _labelSpeed      = nullptr;
    QLabel* _labelHeading    = nullptr;
    QLabel* _labelThrottle   = nullptr;

    // GPS
    QLabel* _labelSats       = nullptr;
    QLabel* _labelHdop       = nullptr;
    QLabel* _labelLatLon     = nullptr;

    // 맵
    MapBridge     _mapBridge;
    QQuickWidget* _mapWidget     = nullptr;
    QQuickWidget* _gebcoWidget   = nullptr;
    TerrainWidget* _terrainWidget = nullptr;
};
