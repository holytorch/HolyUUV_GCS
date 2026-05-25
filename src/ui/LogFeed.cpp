#include "LogFeed.h"
#include "vehicle/VehicleState.h"

#include <QDateTime>
#include <QMetaObject>

LogFeed*         LogFeed::_instance     = nullptr;
QtMessageHandler LogFeed::_prevHandler  = nullptr;


LogFeed::LogFeed(VehicleState* state, QObject* parent)
    : QObject(parent), _state(state)
{
    // 메시지 핸들러는 프로세스 전역 — _instance 첫 LogFeed만 핸들러 설치.
    if (!_instance) {
        _instance    = this;
        _prevHandler = qInstallMessageHandler(_qtMessageHandler);
    }

    if (_state) {
        connect(_state, &VehicleState::armedChanged,
                this, &LogFeed::_onArmedChanged);
        connect(_state, &VehicleState::flightModeChanged,
                this, &LogFeed::_onFlightModeChanged);
        connect(_state, &VehicleState::heartbeatStatusChanged,
                this, &LogFeed::_onHeartbeatStatusChanged);
        connect(_state, &VehicleState::gpsChanged,
                this, &LogFeed::_onGpsChanged);
        connect(_state, &VehicleState::batteryChanged,
                this, &LogFeed::_onBatteryChanged);
    }
}


LogFeed::~LogFeed()
{
    if (_instance == this) {
        qInstallMessageHandler(_prevHandler);
        _instance    = nullptr;
        _prevHandler = nullptr;
    }
}


// qInstallMessageHandler 콜백. 임의 스레드에서 호출될 수 있다.
// 1) 원래 핸들러 호출 → 터미널에 그대로 찍힘
// 2) _instance가 살아 있으면 GUI 스레드로 큐잉해서 append
void LogFeed::_qtMessageHandler(QtMsgType type,
                                const QMessageLogContext& ctx,
                                const QString& msg)
{
    if (_prevHandler) _prevHandler(type, ctx, msg);

    if (!_instance) return;

    const char* tag = "";
    switch (type) {
        case QtDebugMsg:    tag = "DEBUG"; break;
        case QtInfoMsg:     tag = "INFO";  break;
        case QtWarningMsg:  tag = "WARN";  break;
        case QtCriticalMsg: tag = "ERROR"; break;
        case QtFatalMsg:    tag = "FATAL"; break;
    }
    const QString line = QStringLiteral("[%1] %2").arg(QString::fromLatin1(tag), msg);

    // GUI 스레드로 큐잉 (이 함수가 워커 스레드에서 호출될 수 있음).
    QMetaObject::invokeMethod(_instance, "append", Qt::QueuedConnection,
                              Q_ARG(QString, line));
}


void LogFeed::append(const QString& line)
{
    _push(line);
}


void LogFeed::clear()
{
    _lines.clear();
    emit textChanged();
}


QString LogFeed::_timestamp() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}


void LogFeed::_push(const QString& line)
{
    _lines.append(QStringLiteral("[%1] %2").arg(_timestamp(), line));
    while (_lines.size() > MAX_LINES) _lines.removeFirst();
    emit textChanged();
}


// ── VehicleState 슬롯들 ─────────────────────────────────────────────────────

void LogFeed::_onArmedChanged()
{
    _push(_state->armed() ? QStringLiteral("ARMED") : QStringLiteral("DISARMED"));
}


void LogFeed::_onFlightModeChanged()
{
    _push(QStringLiteral("MODE: %1").arg(_state->flightMode()));
}


void LogFeed::_onHeartbeatStatusChanged()
{
    _push(_state->heartbeatOk()
              ? QStringLiteral("LINK OK")
              : QStringLiteral("LINK LOST"));
}


void LogFeed::_onGpsChanged()
{
    // GPS fix 판단: 위성 3개 이상 + 좌표가 (0,0) 아님
    const bool hasFix = _state->gpsSatCount() >= 3
                     && (_state->latitude() != 0.0 || _state->longitude() != 0.0);

    if (hasFix && !_hadGpsFix) {
        _push(QStringLiteral("GPS FIX sats=%1 HDOP=%2")
                  .arg(_state->gpsSatCount())
                  .arg(_state->gpsHdop(), 0, 'f', 1));
        _hadGpsFix = true;
    } else if (!hasFix && _hadGpsFix) {
        _push(QStringLiteral("GPS LOST"));
        _hadGpsFix = false;
    }
}


void LogFeed::_onBatteryChanged()
{
    const int remaining = _state->batteryRemaining();
    if (remaining < 0) return;   // 첫 SYS_STATUS 전

    // 임계점 (떨어지는 방향만 알림): 50 → 30 → 15 → 5 %
    static const int thresholds[] = {50, 30, 15, 5};
    for (int t : thresholds) {
        if (_lastBatteryThreshold > t && remaining <= t) {
            _push(QStringLiteral("BATTERY LOW %1%% (%2 V)")
                      .arg(remaining)
                      .arg(_state->voltage(), 0, 'f', 1));
            break;
        }
    }
    _lastBatteryThreshold = remaining;
}
