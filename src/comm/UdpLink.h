#pragma once

#include "ILink.h"
#include <QUdpSocket>
#include <QHostAddress>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// UdpConfig
// UdpLink 생성에 필요한 로컬 바인드 포트 + (초기 fallback용) 원격 주소.
// QGC 방식: GCS가 localPort에 바인딩해 "수신 대기"하고, 모든 차량 SITL/실기기가
// 이 포트로 push한다. 한 포트로 여러 로봇(발신자)이 들어와 자동 구분된다.
// remoteHost/remotePort는 발신자가 하나도 안 잡혔을 때의 초기 송신 fallback.
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
    // 수신된 모든 발신자(로봇) 엔드포인트. QGC 방식 — 새 발신자가 push해오면
    // 자동 등록하고, 송신(heartbeat/명령)은 등록된 모든 발신자에게 보낸다.
    // MAVLink target_system이 페이로드에 있어, 각 로봇은 자기 대상 메시지만 처리한다.
    struct Endpoint {
        QHostAddress addr;
        quint16      port = 0;
        bool operator==(const Endpoint& o) const { return port == o.port && addr == o.addr; }
    };

    UdpConfig         _config;
    QUdpSocket*       _socket    = nullptr;
    bool              _connected = false;
    QList<Endpoint>   _senders;     // push해온 모든 로봇 (자동 등록)
};
