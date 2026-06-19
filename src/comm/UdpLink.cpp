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
// UDP 데이터그램을 전송한다 (QGC 방식).
// 등록된 발신자(로봇)가 있으면 전원에게 보낸다 — HEARTBEAT은 브로드캐스트,
// 명령은 페이로드의 target_system 덕에 대상 로봇만 처리한다 (다른 로봇은 무시).
// 아직 발신자가 없으면 설정의 remoteHost:remotePort로 fallback (초기 인사용).
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
// 대기 중인 데이터그램을 모두 읽는다. 새 발신자(로봇)가 push해오면 자동 등록해
// 이후 송신 대상에 포함한다 (QGC 방식 — 포트 하나로 여러 로봇 수신).
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
