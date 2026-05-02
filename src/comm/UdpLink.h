#pragma once

#include "ILink.h"
#include <QUdpSocket>
#include <QHostAddress>

struct UdpConfig {
    QString  remoteHost = "192.168.2.1";
    quint16  remotePort = 14550;
    quint16  localPort  = 14550;
};

class UdpLink : public ILink {
    Q_OBJECT
public:
    explicit UdpLink(const UdpConfig& config, QObject* parent = nullptr);
    ~UdpLink() override;

    bool    connectLink() override;
    void    disconnectLink() override;
    bool    isConnected() const override;
    bool    sendBytes(const QByteArray& data) override;
    QString linkName() const override;

private slots:
    void onReadyRead();

private:
    UdpConfig     _config;
    QUdpSocket*   _socket    = nullptr;
    bool          _connected = false;
};
