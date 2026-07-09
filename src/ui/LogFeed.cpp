#include "LogFeed.h"
#include "vehicle/VehicleState.h"

#include <QDateTime>
#include <QMetaObject>

LogFeed*         LogFeed::_instance     = nullptr;
QtMessageHandler LogFeed::_prevHandler  = nullptr;


LogFeed::LogFeed(QObject* parent)
    : QObject(parent)
{
    // The message handler is process-global — only the first LogFeed (_instance)
    // installs it. It must be installed at the very start of boot so that all
    // subsequent [init] logs are captured in the in-app feed too.
    if (!_instance) {
        _instance    = this;
        _prevHandler = qInstallMessageHandler(_qtMessageHandler);
    }

    qInfo("[init] LogFeed");
}


// Connects the active vehicle's state-transition logs. In multi-robot mode this is
// called again when the active vehicle changes, first disconnecting the previous
// vehicle's signals (signal/slot only — nothing network-related).
void LogFeed::bindVehicle(VehicleState* state)
{
    if (_state == state) return;
    if (_state)
        disconnect(_state, nullptr, this, nullptr);   // stop listening to the previous vehicle

    _state = state;
    if (!_state) return;

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


LogFeed::~LogFeed()
{
    qInfo("[exit] LogFeed");
    if (_instance == this) {
        qInstallMessageHandler(_prevHandler);
        _instance    = nullptr;
        _prevHandler = nullptr;
    }
}


// qInstallMessageHandler callback. May be invoked from any thread.
// 1) call the original handler → still printed to the terminal
// 2) if _instance is alive, queue an append onto the GUI thread
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

    // Queue onto the GUI thread (this function may be called from a worker thread).
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


// ── VehicleState slots ──────────────────────────────────────────────────────

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
    // GPS-fix test: at least 3 satellites + coordinates not at (0,0)
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
    if (remaining < 0) return;   // before the first SYS_STATUS

    // Thresholds (only notify while dropping): 50 → 30 → 15 → 5 %
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
