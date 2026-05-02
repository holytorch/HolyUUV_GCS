#pragma once

#include <QObject>
#include <QStandardPaths>

class MapBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(double  latitude     READ latitude     NOTIFY positionChanged)
    Q_PROPERTY(double  longitude    READ longitude    NOTIFY positionChanged)
    Q_PROPERTY(bool    hasPosition  READ hasPosition  NOTIFY positionChanged)
    Q_PROPERTY(QString tileCachePath READ tileCachePath CONSTANT)

public:
    explicit MapBridge(QObject* parent = nullptr) : QObject(parent) {}

    double  latitude()    const { return _lat; }
    double  longitude()   const { return _lon; }
    bool    hasPosition() const { return _hasPos; }
    QString tileCachePath() const {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/tiles";
    }

    void updatePosition(double lat, double lon) {
        _lat = lat; _lon = lon; _hasPos = true;
        emit positionChanged();
    }

    void initCacheDir() const;

signals:
    void positionChanged();

private:
    double _lat = 0.0;
    double _lon = 0.0;
    bool   _hasPos = false;
};
