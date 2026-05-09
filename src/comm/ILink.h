#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// ILink
// 모든 통신 링크(Serial, UDP, TCP)가 구현해야 하는 순수 가상 인터페이스.
// LinkManager는 이 인터페이스만 알고 있어 링크 종류와 무관하게 동작한다.
//
// 흐름:
//   ILink 구현체 → bytesReceived 신호 → LinkManager → MavlinkManager
//   MavlinkManager → 명령 → LinkManager.sendBytes() → ILink 구현체
// ─────────────────────────────────────────────────────────────────────────────
class ILink : public QObject {
    Q_OBJECT
public:
    explicit ILink(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ILink() = default;

    virtual bool connectLink()                      = 0;
    virtual void disconnectLink()                   = 0;
    virtual bool isConnected()              const   = 0;
    virtual bool sendBytes(const QByteArray& data)  = 0;
    virtual QString linkName()              const   = 0;

signals:
    void bytesReceived(const QByteArray& data);
    void linkConnected();
    void linkDisconnected();
    void linkError(const QString& message);
};
