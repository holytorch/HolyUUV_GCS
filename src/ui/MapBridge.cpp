#include "MapBridge.h"
#include <QDir>

// ─────────────────────────────────────────────────────────────────────────────
// initCacheDir()
// tileCachePath()가 가리키는 디렉토리가 없으면 생성한다.
// MainWindow 초기화 시 QML 맵이 캐시를 쓰기 전에 호출된다.
// ─────────────────────────────────────────────────────────────────────────────
void MapBridge::initCacheDir() const {
    QDir().mkpath(tileCachePath());
}
