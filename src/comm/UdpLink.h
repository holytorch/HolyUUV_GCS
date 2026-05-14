#pragma once

#include "ILink.h"
#include <QUdpSocket>
#include <QHostAddress>

// ─────────────────────────────────────────────────────────────────────────────
// UdpConfig
// UdpLink 생성에 필요한 원격 주소와 로컬/원격 포트.
// GCS 기본값: 로컬 14550 포트 바인딩, 차량 192.168.2.1:14550 으로 송신.
// ─────────────────────────────────────────────────────────────────────────────
struct UdpConfig {
    QString  remoteHost = "192.168.2.1";
    quint16  remotePort = 14550;
    quint16  localPort  = 14550;
};

// ─────────────────────────────────────────────────────────────────────────────
// UdpLink
// QUdpSocket 기반 ILink 구현체.
// 시리얼 포트가 감지되지 않을 때 Application이 이 클래스를 생성한다.
// connectLink()에서 로컬 포트를 바인딩하고, sendBytes()로 원격 주소에 데이터그램을 전송한다.
// ─────────────────────────────────────────────────────────────────────────────
class UdpLink : public ILink {
    Q_OBJECT
public:
    explicit UdpLink(const UdpConfig& config, QObject* parent = nullptr);
    ~UdpLink() override;

    bool    connectLink()                       override;
    void    disconnectLink()                    override;
    bool    isConnected()               const   override;
    bool    sendBytes(const QByteArray& data)   override;
    QString linkName()                  const   override;

private slots:
    void onReadyRead();

private:
    UdpConfig     _config;
    QUdpSocket*   _socket    = nullptr;
    bool          _connected = false;

    // 첫 수신 패킷의 발신자 주소를 기억 (QGC와 동일한 방식).
    // SITL이 ephemeral 소스 포트로 텔레메트리를 보내기 때문에
    // 답신은 _config.remoteHost:remotePort가 아니라 실제 발신자에게 보내야 한다.
    // 그렇지 않으면 사용자가 127.0.0.1:14550 입력 시 자기 자신에게 송신되는 버그.
    QHostAddress  _senderAddr;
    quint16       _senderPort    = 0;
    bool          _hasSeenSender = false;
};
