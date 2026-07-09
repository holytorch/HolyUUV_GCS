#pragma once

#include "ILink.h"
#include <QUdpSocket>
#include <QHostAddress>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// UdpConfig
// The local bind port required to create a UdpLink, plus a remote address used
// only as an initial fallback. QGC-style model: the GCS binds localPort and
// "listens", while every vehicle SITL / real device pushes to that port. Multiple
// robots (senders) arriving on one port are distinguished automatically.
// remoteHost/remotePort is the initial send fallback, used only until a sender is
// learned.
// ─────────────────────────────────────────────────────────────────────────────
struct UdpConfig {
    QString  remoteHost = "192.168.2.1";
    quint16  remotePort = 14550;
    quint16  localPort  = 14550;
};

// ─────────────────────────────────────────────────────────────────────────────
// UdpLink
// A QUdpSocket-based implementation of ILink. Application creates this class when
// no serial port is detected. connectLink() binds the local port, and sendBytes()
// transmits datagrams to the remote endpoints.
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
    // Every sender (robot) endpoint we have received from. QGC-style model: a new
    // sender that pushes to us is registered automatically, and outgoing traffic
    // (heartbeat / commands) is sent to all registered senders. Because the MAVLink
    // target_system lives in the payload, each robot processes only the messages
    // addressed to it.
    struct Endpoint {
        QHostAddress addr;
        quint16      port = 0;
        bool operator==(const Endpoint& o) const { return port == o.port && addr == o.addr; }
    };

    UdpConfig         _config;
    QUdpSocket*       _socket    = nullptr;
    bool              _connected = false;
    QList<Endpoint>   _senders;     // every robot that has pushed to us (auto-registered)
};
