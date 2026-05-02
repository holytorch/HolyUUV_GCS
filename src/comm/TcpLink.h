#pragma once

#include "ILink.h"
#include <QTcpSocket>

struct TcpConfig {
    QString  host = "127.0.0.1";
    quint16  port = 5760;
};

class TcpLink : public ILink {
    Q_OBJECT
public:
    explicit TcpLink(const TcpConfig& config, QObject* parent = nullptr);
    ~TcpLink() override;

    bool    connectLink() override;
    void    disconnectLink() override;
    bool    isConnected() const override;
    bool    sendBytes(const QByteArray& data) override;
    QString linkName() const override;

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    TcpConfig    _config;
    QTcpSocket*  _socket = nullptr;
};
