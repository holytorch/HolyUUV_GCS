#pragma once
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// Logger
// Qt 로깅 시스템의 얇은 래퍼. init()으로 타임스탬프 포함 포맷을 설정한다.
// 매크로를 통해 qDebug/qInfo/qWarning/qCritical을 일관된 이름으로 호출한다.
// ─────────────────────────────────────────────────────────────────────────────
namespace Logger {
    inline void init() {
        qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] %{message}");
    }
    inline void shutdown() {}
}

#define LOG_DEBUG(...)  qDebug(__VA_ARGS__)
#define LOG_INFO(...)   qInfo(__VA_ARGS__)
#define LOG_WARN(...)   qWarning(__VA_ARGS__)
#define LOG_ERROR(...)  qCritical(__VA_ARGS__)
