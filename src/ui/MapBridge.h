#pragma once

#include <QObject>
#include <QStandardPaths>

// ─────────────────────────────────────────────────────────────────────────────
// MapBridge
// C++ ↔ QML 브릿지 객체. QML의 MapView / VoyagerView에서 차량 위치와
// 타일 캐시 경로에 접근할 수 있도록 Q_PROPERTY로 노출한다.
//
// QML에서의 사용:
//   bridge.latitude, bridge.longitude — 차량 위치 (GPS 수신 후 갱신)
//   bridge.hasPosition                — GPS 수신 여부
//   bridge.mapCenterLat/Lon           — 사용자가 맵을 이동할 때 QML이 호출로 갱신
//   bridge.tileCachePath              — Qt Location 파일 캐시 기본 경로
// ─────────────────────────────────────────────────────────────────────────────
class MapBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(double  latitude      READ latitude      NOTIFY positionChanged)
    Q_PROPERTY(double  longitude     READ longitude     NOTIFY positionChanged)
    Q_PROPERTY(bool    hasPosition   READ hasPosition   NOTIFY positionChanged)
    Q_PROPERTY(double  mapCenterLat  READ mapCenterLat  NOTIFY mapCenterChanged)
    Q_PROPERTY(double  mapCenterLon  READ mapCenterLon  NOTIFY mapCenterChanged)
    Q_PROPERTY(QString tileCachePath READ tileCachePath CONSTANT)

public:
    explicit MapBridge(QObject* parent = nullptr) : QObject(parent) {}

    double  latitude()     const { return _lat; }
    double  longitude()    const { return _lon; }
    bool    hasPosition()  const { return _hasPos; }
    double  mapCenterLat() const { return _mapCenterLat; }
    double  mapCenterLon() const { return _mapCenterLon; }
    QString tileCachePath() const {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/tiles";
    }

    void updatePosition(double lat, double lon) {
        _lat = lat; _lon = lon; _hasPos = true;
        emit positionChanged();
    }

    Q_INVOKABLE void updateMapCenter(double lat, double lon) {
        _mapCenterLat = lat; _mapCenterLon = lon;
        emit mapCenterChanged();
    }

    void initCacheDir() const;

signals:
    void positionChanged();
    void mapCenterChanged();

private:
    double _lat = 0.0;
    double _lon = 0.0;
    bool   _hasPos = false;
    double _mapCenterLat = 35.074857;
    double _mapCenterLon = 129.084836;
};
