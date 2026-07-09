#include "UdpLink.h"
#include "util/log/logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// UdpLink()
// Creates the QUdpSocket as a child of this object.
// ─────────────────────────────────────────────────────────────────────────────
UdpLink::UdpLink(const UdpConfig& config, QObject* parent)
    : ILink(parent), _config(config)
{
    _socket = new QUdpSocket(this);
}


// ─────────────────────────────────────────────────────────────────────────────
// ~UdpLink()
// Closes the socket on destruction.
// ─────────────────────────────────────────────────────────────────────────────
UdpLink::~UdpLink()
{
    disconnectLink();
}


// ─────────────────────────────────────────────────────────────────────────────
// connectLink()
// Binds the local port and connects the readyRead signal. Since UDP is
// connectionless, a successful bind is itself treated as "connected".
// ─────────────────────────────────────────────────────────────────────────────
bool UdpLink::connectLink()
{
    if (!_socket->bind(QHostAddress::AnyIPv4, _config.localPort)) {
        emit linkError(QString("Failed to bind UDP port %1: %2")
            .arg(_config.localPort).arg(_socket->errorString()));
        return false;
    }

    // Core UDP receive path (event-driven callback).
    connect(_socket, &QUdpSocket::readyRead, this, &UdpLink::onReadyRead);

    _connected = true;
    emit linkConnected();
    LOG_INFO("UDP bound on local port %d → %s:%d",
        _config.localPort,
        qPrintable(_config.remoteHost),
        _config.remotePort);
    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// disconnectLink()
// Closes the socket and emits linkDisconnected.
// ─────────────────────────────────────────────────────────────────────────────
void UdpLink::disconnectLink()
{
    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState)
        _socket->close();

    if (_connected) {
        _connected = false;
        emit linkDisconnected();
        LOG_INFO("UDP disconnected");
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// isConnected()
// ─────────────────────────────────────────────────────────────────────────────
bool UdpLink::isConnected() const
{
    return _connected;
}


// ─────────────────────────────────────────────────────────────────────────────
// sendBytes()
// Transmits a UDP datagram (QGC-style). If any senders (robots) are registered,
// the data is sent to all of them — heartbeats are broadcast, while commands are
// acted on only by the addressed robot thanks to the payload's target_system
// (others ignore it). If no sender has been seen yet, it falls back to the
// configured remoteHost:remotePort (for the initial hello).
// ─────────────────────────────────────────────────────────────────────────────
bool UdpLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;

    if (_senders.isEmpty()) {
        const qint64 sent = _socket->writeDatagram(
            data, QHostAddress(_config.remoteHost), _config.remotePort);
        return sent == data.size();
    }

    bool ok = true;
    for (const Endpoint& ep : _senders)
        if (_socket->writeDatagram(data, ep.addr, ep.port) != data.size())
            ok = false;
    return ok;
}


// ─────────────────────────────────────────────────────────────────────────────
// linkName()
// ─────────────────────────────────────────────────────────────────────────────
QString UdpLink::linkName() const
{
    return QString("UDP[%1:%2]").arg(_config.remoteHost).arg(_config.remotePort);
}


// ─────────────────────────────────────────────────────────────────────────────
// onReadyRead()
// Reads all pending datagrams. When a new sender (robot) pushes to us, it is
// registered automatically so it is included in subsequent transmissions
// (QGC-style — one port receiving many robots).
// ─────────────────────────────────────────────────────────────────────────────
void UdpLink::onReadyRead()
{
    while (_socket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(static_cast<int>(_socket->pendingDatagramSize()));

        QHostAddress sender;
        quint16      senderPort = 0;
        _socket->readDatagram(buf.data(), buf.size(), &sender, &senderPort);

        const Endpoint ep{ sender, senderPort };
        if (!_senders.contains(ep)) {
            _senders.append(ep);
            LOG_INFO("UDP sender registered: %s:%d (total %d)",
                     qPrintable(sender.toString()), senderPort, _senders.size());
        }

        emit bytesReceived(buf);
    }
}
