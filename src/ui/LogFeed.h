#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtMsgHandler>

class VehicleState;

// ─────────────────────────────────────────────────────────────────────────────
// LogFeed
// 두 소스에서 들어오는 로그를 통합해 QML로 노출:
//   1) Qt 메시지 시스템 (qDebug/qInfo/qWarning/qCritical) — qInstallMessageHandler
//      로 가로채서 터미널에 찍히는 모든 로그가 그대로 QML에도 들어감
//   2) VehicleState 신호 — ARMED/MODE/LINK/GPS/BATTERY 등 상태 전이를
//      사람이 읽기 좋은 라인으로 변환
//
// 노출:
//   text       — 누적 로그(여러 줄). textChanged 신호로 QML이 자동 갱신.
//   append()   — 외부에서 임의 라인 push
//   clear()    — 비우기
//
// 정책:
//   - 라인 수가 MAX_LINES를 넘으면 오래된 것부터 drop (ring buffer)
//   - 메시지 핸들러는 thread-safe하지 않은 곳에서도 호출될 수 있어
//     QMetaObject::invokeMethod로 GUI 스레드 큐잉해서 push
//   - 원래 핸들러도 그대로 호출 → 터미널에도 계속 찍힘
// ─────────────────────────────────────────────────────────────────────────────
class LogFeed : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
public:
    explicit LogFeed(QObject* parent = nullptr);
    ~LogFeed() override;

    // VehicleState 신호 연결은 생성 후 따로 바인딩한다. LogFeed를 부팅 가장 앞에서
    // 먼저 만들어 메시지 핸들러로 전체 부팅 로그를 캡처하기 위함 (그 시점엔 아직
    // VehicleState가 생성 전일 수 있음).
    void bindVehicle(VehicleState* state);

    QString text() const { return _lines.join('\n'); }

    // append/clear는 QML/외부 코드용. append는 QtMsgType 핸들러도 사용 (queued).
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

    // qInstallMessageHandler용. 멀티스레드에서 불릴 수 있으니
    // 정적이고 _instance 통해 QMetaObject::invokeMethod로 GUI 스레드로 보낸다.
    static void _qtMessageHandler(QtMsgType type,
                                  const QMessageLogContext& ctx,
                                  const QString& msg);
    static LogFeed*         _instance;
    static QtMessageHandler _prevHandler;

    VehicleState* _state = nullptr;
    QStringList   _lines;

    static constexpr int MAX_LINES = 500;

    // 상태 전이 추적 — 중복 로그 방지용
    bool _hadGpsFix            = false;
    int  _lastBatteryThreshold = 101;
};
