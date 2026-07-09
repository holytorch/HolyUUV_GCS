#pragma once
#include <QDebug>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// Logger
// A thin wrapper over Qt's logging system. init() performs the following:
//   - sets a timestamped message pattern ([hh:mm:ss.zzz] [type] message)
//   - installs a handler that writes to both the console (stderr) and a file
//   - log file: ~/.local/share/HolyUUV_GCS/logs/gcs_<yyyy-MM-dd>.log
//   - rotates by per-file size and by file count
//   - hands the same fd to CrashHandler so crash backtraces are also recorded
//     in this file
// shutdown() restores the previous handler and closes the file.
// ─────────────────────────────────────────────────────────────────────────────
namespace Logger {
    void init();
    void shutdown();
    QString logFilePath();   // path of the currently open log file (empty if open failed)
}

#define LOG_DEBUG(...)  qDebug(__VA_ARGS__)
#define LOG_INFO(...)   qInfo(__VA_ARGS__)
