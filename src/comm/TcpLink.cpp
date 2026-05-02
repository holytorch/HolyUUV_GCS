#include "TcpLink.h"
#include "util/log/logger.h"

TcpLink::TcpLink(const TcpConfig& config, QObject* parent)
    : ILink(parent), _config(config)
{
    _socket = new QTcpSocket(this);

    connect(_socket, &QTcpSocket::connected,    this, &TcpLink::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &TcpLink::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead,    this, &TcpLink::onReadyRead);
    connect(_socket, &QAbstractSocket::errorOccurred, this, &TcpLink::onErrorOccurred);
}

TcpLink::~TcpLink()
{
    disconnectLink();
}

bool TcpLink::connectLink()
{
    _socket->connectToHost(_config.host, _config.port);
    LOG_INFO("TCP connecting to %s:%d ...", qPrintable(_config.host), _config.port);
    return true;
}

void TcpLink::disconnectLink()
{
    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->disconnectFromHost();
    }
}

bool TcpLink::isConnected() const
{
    return _socket && _socket->state() == QAbstractSocket::ConnectedState;
}

bool TcpLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;
    return _socket->write(data) == data.size();
}

QString TcpLink::linkName() const
{
    return QString("TCP[%1:%2]").arg(_config.host).arg(_config.port);
}

void TcpLink::onConnected()
{
    LOG_INFO("TCP connected: %s:%d", qPrintable(_config.host), _config.port);
    emit linkConnected();
}

void TcpLink::onDisconnected()
{
    LOG_INFO("TCP disconnected");
    emit linkDisconnected();
}

void TcpLink::onReadyRead()
{
    const QByteArray data = _socket->readAll();
    if (!data.isEmpty()) {
        emit bytesReceived(data);
    }
}

void TcpLink::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit linkError(QString("TCP error: %1").arg(_socket->errorString()));
}
