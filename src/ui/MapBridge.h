#pragma once

#include <QObject>
#include <QStandardPaths>

// ─────────────────────────────────────────────────────────────────────────────
// MapBridge
// A C++ ↔ QML bridge object. Exposes the vehicle position and tile-cache path via
// Q_PROPERTY so QML's MapView / VoyagerView can access them.
//
// Usage from QML:
//   bridge.latitude, bridge.longitude — vehicle position (updated after a GPS fix)
//   bridge.hasPosition                — whether a GPS fix has been received
//   bridge.mapCenterLat/Lon           — updated by QML when the user pans the map
//   bridge.tileCachePath              — default path for the Qt Location file cache
// ─────────────────────────────────────────────────────────────────────────────
class MapBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(double  latitude      READ latitude      NOTIFY positionChanged)
    Q_PROPERTY(double  longitude     READ longitude     NOTIFY positionChanged)
    Q_PROPERTY(bool    hasPosition   READ hasPosition   NOTIFY positionChanged)
    Q_PROPERTY(double  mapCenterLat  READ mapCenterLat  NOTIFY mapCenterChanged)
    Q_PROPERTY(double  mapCenterLon  READ mapCenterLon  NOTIFY mapCenterChanged)
    Q_PROPERTY(QString tileCachePath READ tileCachePath CONSTANT)
    Q_PROPERTY(QString mapMode       READ mapMode       NOTIFY mapModeChanged)

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
    QString mapMode() const { return _mapMode; }

    void updatePosition(double lat, double lon) {
        _lat = lat; _lon = lon; _hasPos = true;
        emit positionChanged();
    }

    Q_INVOKABLE void updateMapCenter(double lat, double lon) {
        _mapCenterLat = lat; _mapCenterLon = lon;
        emit mapCenterChanged();
    }

    // Notifies a QML map-mode change ("osm" / "voyager" / "3d"). Used by the
    // Mission tab's 3D-stack toggle.
    Q_INVOKABLE void setMapMode(const QString& mode) {
        if (_mapMode == mode) return;
        _mapMode = mode;
        emit mapModeChanged(mode);
    }

    void initCacheDir() const;

signals:
    void positionChanged();
    void mapCenterChanged();
    void mapModeChanged(const QString& mode);

private:
    double  _lat = 0.0;
    double  _lon = 0.0;
    bool    _hasPos = false;
    double  _mapCenterLat = 37.52951029463262;
    double  _mapCenterLon = 126.94149832867085;
    QString _mapMode = "osm";
};
