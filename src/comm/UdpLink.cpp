#include "UdpLink.h"
#include "util/log/logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// UdpLink()
// QUdpSocket 객체를 this의 자식으로 생성한다.
// ─────────────────────────────────────────────────────────────────────────────
UdpLink::UdpLink(const UdpConfig& config, QObject* parent)
    : ILink(parent), _config(config)
{
    _socket = new QUdpSocket(this);
}


// ─────────────────────────────────────────────────────────────────────────────
// ~UdpLink()
// 소멸 시 소켓을 닫는다.
// ─────────────────────────────────────────────────────────────────────────────
UdpLink::~UdpLink()
{
    disconnectLink();
}


// ─────────────────────────────────────────────────────────────────────────────
// connectLink()
// 로컬 포트를 바인딩하고 readyRead 시그널을 연결한다.
// UDP는 연결형이 아니므로 바인딩 성공 자체를 "연결됨"으로 처리한다.
// ─────────────────────────────────────────────────────────────────────────────
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


// ─────────────────────────────────────────────────────────────────────────────
// disconnectLink()
// 소켓을 닫고 linkDisconnected를 발신한다.
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
// 원격 주소로 UDP 데이터그램을 전송한다.
// ─────────────────────────────────────────────────────────────────────────────
bool UdpLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;
    const qint64 sent = _socket->writeDatagram(
        data, QHostAddress(_config.remoteHost), _config.remotePort);
    return sent == data.size();
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
// 대기 중인 모든 데이터그램을 순서대로 읽어 bytesReceived 신호로 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void UdpLink::onReadyRead()
{
    while (_socket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(static_cast<int>(_socket->pendingDatagramSize()));
        _socket->readDatagram(buf.data(), buf.size());
        emit bytesReceived(buf);
    }
}
