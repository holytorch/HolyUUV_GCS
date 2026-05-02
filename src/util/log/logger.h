#pragma once
#include <QDebug>

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
