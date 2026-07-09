#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// VehicleState
// Stores all vehicle telemetry parsed from MAVLink messages and emits change
// notifications. MavlinkManager calls the update*() slots, and MainWindow (QML)
// reacts to the signals to refresh the UI.
//
// Data groups:
//   Battery   — SYS_STATUS message
//   Attitude  — ATTITUDE message (rad)
//   Navigation— VFR_HUD message (altitude = depth, negative)
//   GPS       — GLOBAL_POSITION_INT + GPS_RAW_INT messages
//   Flight    — HEARTBEAT message (armed, flight mode, connection watchdog)
// ─────────────────────────────────────────────────────────────────────────────
class VehicleState : public QObject {
    Q_OBJECT
    // For QML binding — the getter signatures are unchanged, only annotations added.
    Q_PROPERTY(int     sysid            READ sysid            NOTIFY sysidChanged)
    Q_PROPERTY(int     batteryRemaining READ batteryRemaining NOTIFY batteryChanged)
    Q_PROPERTY(float   voltage          READ voltage          NOTIFY batteryChanged)
    Q_PROPERTY(float   current          READ current          NOTIFY batteryChanged)

    Q_PROPERTY(float   roll             READ roll             NOTIFY attitudeChanged)
    Q_PROPERTY(float   pitch            READ pitch            NOTIFY attitudeChanged)
    Q_PROPERTY(float   yaw              READ yaw              NOTIFY attitudeChanged)

    Q_PROPERTY(float   depth            READ depth            NOTIFY vfrHudChanged)
    Q_PROPERTY(float   groundspeed      READ groundspeed      NOTIFY vfrHudChanged)
    Q_PROPERTY(float   heading          READ heading          NOTIFY vfrHudChanged)
    Q_PROPERTY(int     throttle         READ throttle         NOTIFY vfrHudChanged)

    Q_PROPERTY(double  latitude         READ latitude         NOTIFY gpsChanged)
    Q_PROPERTY(double  longitude        READ longitude        NOTIFY gpsChanged)
    Q_PROPERTY(int     gpsSatCount      READ gpsSatCount      NOTIFY gpsChanged)
    Q_PROPERTY(float   gpsHdop          READ gpsHdop          NOTIFY gpsChanged)

    Q_PROPERTY(bool    armed            READ armed            NOTIFY armedChanged)
    Q_PROPERTY(QString flightMode       READ flightMode       NOTIFY flightModeChanged)
    Q_PROPERTY(bool    heartbeatOk      READ heartbeatOk      NOTIFY heartbeatStatusChanged)

    Q_PROPERTY(quint16 dropRateComm     READ dropRateComm     NOTIFY linkQualityChanged)
    Q_PROPERTY(quint16 errorsComm       READ errorsComm       NOTIFY linkQualityChanged)
    Q_PROPERTY(quint8  rssi             READ rssi             NOTIFY linkQualityChanged)
    Q_PROPERTY(quint8  remRssi          READ remRssi          NOTIFY linkQualityChanged)
    Q_PROPERTY(bool    hasRadioStatus   READ hasRadioStatus   NOTIFY linkQualityChanged)

public:
    explicit VehicleState(QObject* parent = nullptr);
    // Multi-robot: one object per robot. The sysid is fixed at construction, and
    // on disconnect/timeout the whole object is deleted (object lifetime = the
    // robot's lifetime). The number is never re-assigned.
    explicit VehicleState(int sysid, QObject* parent = nullptr);
    ~VehicleState() override;

    int    sysid()            const { return _sysid; }
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

    bool    armed()           const { return _armed; }
    QString flightMode()      const { return _flightMode; }
    bool    heartbeatOk()     const { return _heartbeatOk; }

    uint16_t dropRateComm()   const { return _dropRateComm; }
    uint16_t errorsComm()     const { return _errorsComm; }
    uint8_t  rssi()           const { return _rssi; }
    uint8_t  remRssi()        const { return _remRssi; }
    bool     hasRadioStatus() const { return _hasRadioStatus; }

public slots:
    void updateBattery(int remaining, float voltage, float current, uint16_t dropRate, uint16_t errorsComm);
    void updateAttitude(float roll, float pitch, float yaw);
    void updateVfrHud(float groundspeed, float depth, float heading, int throttle);
    void updateGlobalPosition(double lat, double lon);
    void updateGpsRaw(int satCount, float hdop);
    void updateHeartbeat(bool armed, uint32_t customMode);
    void updateRadioStatus(uint8_t rssi, uint8_t remRssi);
    // Called when the active vehicle changes. Resets all telemetry to defaults to
    // avoid showing stale values.
    void setSysid(int sysid);
    void resetTelemetry();

signals:
    void batteryChanged();
    void attitudeChanged();
    void vfrHudChanged();
    void gpsChanged();
    void armedChanged();
    void flightModeChanged();
    void heartbeatStatusChanged();
    void linkQualityChanged();
    void sysidChanged();

private:
    int    _sysid            = 0;   // 0 = undetermined (before the first HEARTBEAT)

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

    bool    _armed           = false;
    QString _flightMode      = "Unknown";
    bool    _heartbeatOk     = false;
    QTimer* _watchdog        = nullptr;
    QElapsedTimer _heartbeatElapsed;

    uint16_t _dropRateComm   = 0;
    uint16_t _errorsComm     = 0;
    uint8_t  _rssi           = 0;
    uint8_t  _remRssi        = 0;
    bool     _hasRadioStatus = false;
};
