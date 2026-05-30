#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// VehicleState
// MAVLink 메시지에서 파싱된 모든 차량 텔레메트리를 저장하고 변경 알림을 발신한다.
// MavlinkManager가 update*() 슬롯을 호출하고, MainWindow(QML)이 신호를 받아 UI를 갱신한다.
//
// 데이터 그룹:
//   배터리   — SYS_STATUS 메시지
//   자세     — ATTITUDE 메시지 (rad)
//   항법     — VFR_HUD 메시지 (altitude = 수심, 음수)
//   GPS      — GLOBAL_POSITION_INT + GPS_RAW_INT 메시지
//   비행상태 — HEARTBEAT 메시지 (armed, 비행모드, 연결 감시)
// ─────────────────────────────────────────────────────────────────────────────
class VehicleState : public QObject {
    Q_OBJECT
    // QML 바인딩용 — getter 시그니처는 그대로 두고 어노테이션만 추가.
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
    // 활성 차량이 바뀌면 호출. 모든 telemetry를 초기값으로 리셋해 stale 표시 방지.
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
    int    _sysid            = 0;   // 0 = 미정 (HEARTBEAT 받기 전)

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
