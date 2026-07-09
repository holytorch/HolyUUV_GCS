#include "MapBridge.h"
#include <QDir>

// ─────────────────────────────────────────────────────────────────────────────
// initCacheDir()
// Creates the directory that tileCachePath() points to if it does not exist.
// Called during MainWindow initialization, before the QML map uses the cache.
// ─────────────────────────────────────────────────────────────────────────────
void MapBridge::initCacheDir() const {
    QDir().mkpath(tileCachePath());
}
