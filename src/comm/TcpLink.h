#pragma once

#include "ILink.h"
#include <QTcpSocket>

// ─────────────────────────────────────────────────────────────────────────────
// TcpConfig
// TcpLink 생성에 필요한 원격 호스트 주소와 포트.
// ─────────────────────────────────────────────────────────────────────────────
struct TcpConfig {
    QString  host = "127.0.0.1";
    quint16  port = 5760;
};

// ─────────────────────────────────────────────────────────────────────────────
// TcpLink
// QTcpSocket 기반 ILink 구현체.
// connectLink()는 비동기 연결을 시작하고 connected 시그널이 오면 linkConnected를 발신한다.
// 시뮬레이터(예: ArduSub SITL)처럼 TCP 소켓을 제공하는 대상에 연결할 때 사용한다.
// ─────────────────────────────────────────────────────────────────────────────
class TcpLink : public ILink {
    Q_OBJECT
public:
    explicit TcpLink(const TcpConfig& config, QObject* parent = nullptr);
    ~TcpLink() override;

    bool    connectLink()                       override;
    void    disconnectLink()                    override;
    bool    isConnected()               const   override;
    bool    sendBytes(const QByteArray& data)   override;
    QString linkName()                  const   override;

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    TcpConfig    _config;
    QTcpSocket*  _socket = nullptr;
};
