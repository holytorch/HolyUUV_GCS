#include "UdpLink.h"
#include "util/log/logger.h"

UdpLink::UdpLink(const UdpConfig& config, QObject* parent)
    : ILink(parent), _config(config)
{
    _socket = new QUdpSocket(this);
}

UdpLink::~UdpLink()
{
    disconnectLink();
}

bool UdpLink::connectLink()
{
    if (!_socket->bind(QHostAddress::AnyIPv4, _config.localPort)) {
        emit linkError(QString("Failed to bind UDP port %1: %2")
            .arg(_config.localPort).arg(_socket->errorString()));
        return false;
    }

    connect(_socket, &QUdpSocket::readyRead, this, &UdpLink::onReadyRead);

    _connected = true;
    emit linkConnected();
    LOG_INFO("UDP bound on local port %d → %s:%d",
        _config.localPort,
        qPrintable(_config.remoteHost),
        _config.remotePort);
    return true;
}

void UdpLink::disconnectLink()
{
    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->close();
    }
    if (_connected) {
        _connected = false;
        emit linkDisconnected();
        LOG_INFO("UDP disconnected");
    }
}

bool UdpLink::isConnected() const
{
    return _connected;
}

bool UdpLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;
    const qint64 sent = _socket->writeDatagram(
        data, QHostAddress(_config.remoteHost), _config.remotePort);
    return sent == data.size();
}

QString UdpLink::linkName() const
{
    return QString("UDP[%1:%2]").arg(_config.remoteHost).arg(_config.remotePort);
}

void UdpLink::onReadyRead()
{
    while (_socket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(static_cast<int>(_socket->pendingDatagramSize()));
        _socket->readDatagram(buf.data(), buf.size());
        emit bytesReceived(buf);
    }
}
