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

    // UDP 수신 핵심 코드 (이벤트 기반 콜백)
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
// UDP 데이터그램을 전송한다.
// 한 번이라도 패킷을 받았으면 그 발신자에게 답신 (SITL/MAVProxy의 ephemeral 포트 대응),
// 아직 못 받았으면 설정의 remoteHost:remotePort로 fallback (초기 인사용).
// ─────────────────────────────────────────────────────────────────────────────
bool UdpLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;

    const QHostAddress addr = _hasSeenSender ? _senderAddr  : QHostAddress(_config.remoteHost);
    const quint16      port = _hasSeenSender ? _senderPort  : _config.remotePort;

    const qint64 sent = _socket->writeDatagram(data, addr, port);
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
// 대기 중인 데이터그램을 모두 읽고, 첫 패킷의 발신자 주소를 기억한다.
// 기억된 주소는 sendBytes()가 답신할 때 사용한다 (자동 sender latch).
// ─────────────────────────────────────────────────────────────────────────────
void UdpLink::onReadyRead()
{
    while (_socket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(static_cast<int>(_socket->pendingDatagramSize()));

        QHostAddress sender;
        quint16      senderPort = 0;
        _socket->readDatagram(buf.data(), buf.size(), &sender, &senderPort);

        if (!_hasSeenSender) {
            _senderAddr    = sender;
            _senderPort    = senderPort;
            _hasSeenSender = true;
            LOG_INFO("UDP sender locked: %s:%d",
                     qPrintable(sender.toString()), senderPort);
        }

        emit bytesReceived(buf);
    }
}
