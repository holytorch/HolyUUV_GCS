#pragma once

#include <QObject>

// ─────────────────────────────────────────────────────────────────────────────
// VehicleState
// MAVLink 메시지에서 파싱된 모든 차량 텔레메트리를 저장하고 변경 알림을 발신한다.
// MavlinkManager가 update*() 슬롯을 호출하고, MainWindow가 변경 신호를 받아 UI를 갱신한다.
//
// 데이터 그룹:
//   배터리 — SYS_STATUS 메시지
//   자세   — ATTITUDE 메시지 (rad)
//   항법   — VFR_HUD 메시지 (altitude = 수심, 음수)
//   GPS    — GLOBAL_POSITION_INT + GPS_RAW_INT 메시지
// ─────────────────────────────────────────────────────────────────────────────
class VehicleState : public QObject {
    Q_OBJECT
public:
    explicit VehicleState(QObject* parent = nullptr);

    int    batteryRemaining() const { return _batteryRemaining; }
    float  voltage()          const { return _voltage; }
    float  current()          const { return _current; }

    float  roll()             const { return _roll; }
    float  pitch()            const { return _pitch; }
    float  yaw()              const { return _yaw; }

    float  depth()            const { return _depth; }
    float  groundspeed()      const { return _groundspeed; }
    float  heading()          const { return _heading; }
    int    throttle()         const { return _throttle; }

    double latitude()         const { return _latitude; }
    double longitude()        const { return _longitude; }
    int    gpsSatCount()      const { return _gpsSatCount; }
    float  gpsHdop()          const { return _gpsHdop; }

public slots:
    void updateBattery(int remaining, float voltage, float current);
    void updateAttitude(float roll, float pitch, float yaw);
    void updateVfrHud(float groundspeed, float depth, float heading, int throttle);
    void updateGlobalPosition(double lat, double lon);
    void updateGpsRaw(int satCount, float hdop);

signals:
    void batteryChanged();
    void attitudeChanged();
    void vfrHudChanged();
    void gpsChanged();

private:
    int    _batteryRemaining = -1;
    float  _voltage          = 0.0f;
    float  _current          = 0.0f;

    float  _roll             = 0.0f;
    float  _pitch            = 0.0f;
    float  _yaw              = 0.0f;

    float  _depth            = 0.0f;
    float  _groundspeed      = 0.0f;
    float  _heading          = 0.0f;
    int    _throttle         = 0;

    double _latitude         = 0.0;
    double _longitude        = 0.0;
    int    _gpsSatCount      = 0;
    float  _gpsHdop          = 99.9f;
};
