#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtMsgHandler>

class VehicleState;

// ─────────────────────────────────────────────────────────────────────────────
// LogFeed
// Merges logs from two sources and exposes them to QML:
//   1) The Qt message system (qDebug/qInfo/qWarning/qCritical) — intercepted with
//      qInstallMessageHandler, so every log printed to the terminal also flows
//      into QML.
//   2) VehicleState signals — converts state transitions (ARMED/MODE/LINK/GPS/
//      BATTERY, etc.) into human-readable lines.
//
// Exposes:
//   text       — the accumulated log (multi-line). QML refreshes automatically via
//                textChanged.
//   append()   — push an arbitrary line from outside
//   clear()    — clear the feed
//
// Policy:
//   - once the line count exceeds MAX_LINES, the oldest are dropped (ring buffer)
//   - the message handler may be invoked where it is not thread-safe, so lines are
//     queued onto the GUI thread with QMetaObject::invokeMethod
//   - the original handler is also invoked → the terminal keeps printing too
// ─────────────────────────────────────────────────────────────────────────────
class LogFeed : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
public:
    explicit LogFeed(QObject* parent = nullptr);
    ~LogFeed() override;

    // The VehicleState signal connections are bound separately after construction.
    // LogFeed is created at the very start of boot so its message handler captures
    // the entire boot log (at which point VehicleState may not exist yet).
    void bindVehicle(VehicleState* state);

    QString text() const { return _lines.join('\n'); }

    // append/clear are for QML/external code. append is also used by the QtMsgType
    // handler (queued).
    Q_INVOKABLE void append(const QString& line);
    Q_INVOKABLE void clear();

signals:
    void textChanged();

private slots:
    void _onArmedChanged();
    void _onFlightModeChanged();
    void _onHeartbeatStatusChanged();
    void _onGpsChanged();
    void _onBatteryChanged();

private:
    QString _timestamp() const;
    void    _push(const QString& line);

    // For qInstallMessageHandler. It may be called from multiple threads, so it is
    // static and routes to the GUI thread via _instance and QMetaObject::invokeMethod.
    static void _qtMessageHandler(QtMsgType type,
                                  const QMessageLogContext& ctx,
                                  const QString& msg);
    static LogFeed*         _instance;
    static QtMessageHandler _prevHandler;

    VehicleState* _state = nullptr;
    QStringList   _lines;

    static constexpr int MAX_LINES = 500;

    // State-transition tracking — to avoid duplicate log lines
    bool _hadGpsFix            = false;
    int  _lastBatteryThreshold = 101;
};
