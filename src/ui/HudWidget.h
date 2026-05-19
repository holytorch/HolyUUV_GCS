#pragma once

#include <QWidget>
#include <QLabel>
#include "vehicle/VehicleState.h"

class AttitudeIndicator;
class CompassWidget;
class DepthGauge;

// ─────────────────────────────────────────────────────────────────────────────
// HudWidget
// AttitudeIndicator / CompassWidget / DepthGauge를 하나의 탭으로 묶고
// VehicleState 변경 신호를 각 위젯에 연결하는 컨테이너.
//
// 레이아웃:
//   상단 상태바: [ ARMED/DISARMED ] [ 비행모드 ] [ Heartbeat ]
//   계기:        [ AttitudeIndicator (280×280) ] [ CompassWidget (200×200) ] [ DepthGauge (80×280) ]
// ─────────────────────────────────────────────────────────────────────────────
class HudWidget : public QWidget {
    Q_OBJECT
public:
    explicit HudWidget(VehicleState* state, QWidget* parent = nullptr);

public slots:
    void onAttitudeChanged();
    void onVfrHudChanged();
    void onArmedChanged();
    void onFlightModeChanged();
    void onHeartbeatStatusChanged();
    void onBatteryChanged();
    void onLinkQualityChanged();

private:
    VehicleState*      _state       = nullptr;
    AttitudeIndicator* _attitude    = nullptr;
    CompassWidget*     _compass     = nullptr;
    DepthGauge*        _depth       = nullptr;
    QLabel*            _lblArmed    = nullptr;
    QLabel*            _lblMode     = nullptr;
    QLabel*            _lblHb       = nullptr;
    QLabel*            _lblBattery  = nullptr;
    QLabel*            _lblLink     = nullptr;
};
