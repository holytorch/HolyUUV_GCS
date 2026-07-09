#include "logger.h"
#include "crash_handler.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QMutex>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// ─────────────────────────────────────────────────────────────────────────────
// File logging: mirrors console (stderr) output into ~/.local/share/HolyUUV_GCS/logs/.
//   - filename   : gcs_<yyyy-MM-dd>.log
//   - rotation   : when a file exceeds kMaxBytes it is archived with a timestamp
//                  suffix and a fresh file is opened; the logs/ directory keeps
//                  only the kMaxFiles most recent files
//   - writes     : ::write (unbuffered) — even the last message before a crash is
//                  not lost
//   - crash link : the same fd is handed to CrashHandler so backtraces are
//                  recorded in this file too
// The message handler may be invoked concurrently from multiple threads, so it is
// guarded by a QMutex.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

constexpr qint64 kMaxBytes = 5 * 1024 * 1024;   // 5 MB per file
constexpr int    kMaxFiles = 10;                // files retained in logs/

QMutex           g_mutex;
int              g_fd          = -1;
qint64           g_written     = 0;
QString          g_logDir;
QtMessageHandler g_prevHandler = nullptr;

QString activePath()
{
    return g_logDir + "/gcs_" + QDate::currentDate().toString("yyyy-MM-dd") + ".log";
}

// Keep only the kMaxFiles most recent files in logs/, deleting older ones.
void pruneOldLogs()
{
    const QFileInfoList files = QDir(g_logDir).entryInfoList(
        QStringList() << "gcs_*.log", QDir::Files, QDir::Time);   // newest first
    for (int i = kMaxFiles; i < files.size(); ++i)
        QFile::remove(files[i].absoluteFilePath());
}

void openActive()
{
    const QByteArray path = activePath().toLocal8Bit();
    g_fd = ::open(path.constData(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    struct stat st {};
    g_written = (g_fd >= 0 && ::fstat(g_fd, &st) == 0) ? static_cast<qint64>(st.st_size) : 0;
}

// When the file exceeds its size cap, archive it with a timestamp suffix and open
// a fresh one (the caller holds g_mutex).
void rotateIfNeeded()
{
    if (g_fd < 0 || g_written < kMaxBytes) return;
    ::close(g_fd);
    g_fd = -1;
    const QString rolled = g_logDir + "/gcs_"
        + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmsszzz") + ".log";
    QFile::rename(activePath(), rolled);
    openActive();
    CrashHandler::setLogFd(g_fd);   // update to the new fd
    pruneOldLogs();
}

void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const QByteArray line =
        (qFormatLogMessage(type, ctx, msg) + QLatin1Char('\n')).toUtf8();

    QMutexLocker lock(&g_mutex);
    const ssize_t r1 = ::write(STDERR_FILENO, line.constData(), line.size()); (void)r1;
    if (g_fd >= 0) {
        const ssize_t r2 = ::write(g_fd, line.constData(), line.size()); (void)r2;
        g_written += line.size();
        rotateIfNeeded();
    }
}

} // namespace


void Logger::init()
{
    qSetMessagePattern("[%{time hh:mm:ss.zzz}] [%{type}] %{message}");

    // Called after applicationName("HolyUUV_GCS") is set → ~/.local/share/HolyUUV_GCS/logs
    g_logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(g_logDir);
    pruneOldLogs();
    openActive();

    if (g_fd >= 0)
        CrashHandler::setLogFd(g_fd);   // record crash backtraces in the same file

    g_prevHandler = qInstallMessageHandler(messageHandler);
}


void Logger::shutdown()
{
    QMutexLocker lock(&g_mutex);
    if (g_prevHandler) {
        qInstallMessageHandler(g_prevHandler);
        g_prevHandler = nullptr;
    }
    CrashHandler::setLogFd(-1);
    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
}


QString Logger::logFilePath()
{
    QMutexLocker lock(&g_mutex);
    return (g_fd >= 0) ? activePath() : QString();
}
