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
// 파일 로깅: 콘솔(stderr)과 동시에 ~/.local/share/HolyUUV_GCS/logs/ 에 기록한다.
//   - 파일명     : gcs_<yyyy-MM-dd>.log
//   - 로테이션   : 파일이 kMaxBytes 초과 시 타임스탬프 붙여 보관 후 새 파일,
//                  logs/ 디렉터리는 가장 최근 kMaxFiles 개만 유지
//   - 쓰기       : ::write (버퍼 없음) — 크래시 직전 메시지도 유실되지 않음
//   - 크래시 연동 : 같은 fd를 CrashHandler에 넘겨 백트레이스도 이 파일에 함께 남긴다
// 메시지 핸들러는 여러 스레드에서 동시 호출될 수 있어 QMutex로 보호한다.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

constexpr qint64 kMaxBytes = 5 * 1024 * 1024;   // 파일당 5 MB
constexpr int    kMaxFiles = 10;                // logs/ 보관 개수

QMutex           g_mutex;
int              g_fd          = -1;
qint64           g_written     = 0;
QString          g_logDir;
QtMessageHandler g_prevHandler = nullptr;

QString activePath()
{
    return g_logDir + "/gcs_" + QDate::currentDate().toString("yyyy-MM-dd") + ".log";
}

// logs/ 에서 가장 최근 kMaxFiles 개만 남기고 오래된 파일 삭제.
void pruneOldLogs()
{
    const QFileInfoList files = QDir(g_logDir).entryInfoList(
        QStringList() << "gcs_*.log", QDir::Files, QDir::Time);   // 최신 우선
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

// 용량 초과 시 현재 파일을 타임스탬프 붙여 보관하고 새 파일을 연다 (호출자가 g_mutex 보유).
void rotateIfNeeded()
{
    if (g_fd < 0 || g_written < kMaxBytes) return;
    ::close(g_fd);
    g_fd = -1;
    const QString rolled = g_logDir + "/gcs_"
        + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmsszzz") + ".log";
    QFile::rename(activePath(), rolled);
    openActive();
    CrashHandler::setLogFd(g_fd);   // 새 fd 로 갱신
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

    // applicationName("HolyUUV_GCS") 설정 후 호출되므로 → ~/.local/share/HolyUUV_GCS/logs
    g_logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(g_logDir);
    pruneOldLogs();
    openActive();

    if (g_fd >= 0)
        CrashHandler::setLogFd(g_fd);   // 크래시 백트레이스도 같은 파일에 기록

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
