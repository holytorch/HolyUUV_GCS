#pragma once

#include <QObject>

class VehicleState : public QObject {
    Q_OBJECT
public:
    explicit VehicleState(QObject* parent = nullptr);

    // 배터리
    int     batteryRemaining() const { return _batteryRemaining; }
    float   voltage()          const { return _voltage; }
    float   current()          const { return _current; }

    // 자세
    float   roll()             const { return _roll; }
    float   pitch()            const { return _pitch; }
    float   yaw()              const { return _yaw; }

    // 위치/이동
    float   depth()            const { return _depth; }
    float   groundspeed()      const { return _groundspeed; }
    float   heading()          const { return _heading; }
    int     throttle()         const { return _throttle; }

    // GPS
    double  latitude()         const { return _latitude; }
    double  longitude()        const { return _longitude; }
    int     gpsSatCount()      const { return _gpsSatCount; }
    float   gpsHdop()          const { return _gpsHdop; }

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
