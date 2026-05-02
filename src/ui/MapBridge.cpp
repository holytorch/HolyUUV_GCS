#include "MapBridge.h"
#include <QDir>

// 캐시 디렉토리가 없으면 생성
void MapBridge::initCacheDir() const {
    QDir().mkpath(tileCachePath());
}
