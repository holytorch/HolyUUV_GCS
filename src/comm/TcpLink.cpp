#include "TcpLink.h"
#include "util/log/logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// TcpLink()
// QTcpSocket을 생성하고 connected / disconnected / readyRead / errorOccurred
// 시그널을 각 슬롯에 미리 연결한다.
// ─────────────────────────────────────────────────────────────────────────────
TcpLink::TcpLink(const TcpConfig& config, QObject* parent)
    : ILink(parent), _config(config)
{
    _socket = new QTcpSocket(this);

    connect(_socket, &QTcpSocket::connected,    this, &TcpLink::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &TcpLink::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead,    this, &TcpLink::onReadyRead);
    connect(_socket, &QAbstractSocket::errorOccurred, this, &TcpLink::onErrorOccurred);
}


// ─────────────────────────────────────────────────────────────────────────────
// ~TcpLink()
// 소멸 시 소켓 연결을 해제한다.
// ─────────────────────────────────────────────────────────────────────────────
TcpLink::~TcpLink()
{
    disconnectLink();
}


// ─────────────────────────────────────────────────────────────────────────────
// connectLink()
// 비동기 TCP 연결을 시작한다. 연결 완료는 onConnected() 슬롯에서 처리된다.
// ─────────────────────────────────────────────────────────────────────────────
bool TcpLink::connectLink()
{
    _socket->connectToHost(_config.host, _config.port);
    LOG_INFO("TCP connecting to %s:%d ...", qPrintable(_config.host), _config.port);
    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// disconnectLink()
// 소켓이 연결 중이거나 연결된 상태이면 연결 해제를 요청한다.
// ─────────────────────────────────────────────────────────────────────────────
void TcpLink::disconnectLink()
{
    if (_socket && _socket->state() != QAbstractSocket::UnconnectedState)
        _socket->disconnectFromHost();
}


// ─────────────────────────────────────────────────────────────────────────────
// isConnected()
// ─────────────────────────────────────────────────────────────────────────────
bool TcpLink::isConnected() const
{
    return _socket && _socket->state() == QAbstractSocket::ConnectedState;
}


// ─────────────────────────────────────────────────────────────────────────────
// sendBytes()
// TCP 스트림에 데이터를 쓴다. 전송 바이트 수가 일치하면 true를 반환한다.
// ─────────────────────────────────────────────────────────────────────────────
bool TcpLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;
    return _socket->write(data) == data.size();
}


// ─────────────────────────────────────────────────────────────────────────────
// linkName()
// ─────────────────────────────────────────────────────────────────────────────
QString TcpLink::linkName() const
{
    return QString("TCP[%1:%2]").arg(_config.host).arg(_config.port);
}


// ─────────────────────────────────────────────────────────────────────────────
// onConnected()
// TCP 연결이 성립되면 linkConnected를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void TcpLink::onConnected()
{
    LOG_INFO("TCP connected: %s:%d", qPrintable(_config.host), _config.port);
    emit linkConnected();
}


// ─────────────────────────────────────────────────────────────────────────────
// onDisconnected()
// 소켓이 끊기면 linkDisconnected를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void TcpLink::onDisconnected()
{
    LOG_INFO("TCP disconnected");
    emit linkDisconnected();
}


// ─────────────────────────────────────────────────────────────────────────────
// onReadyRead()
// 수신 버퍼의 모든 데이터를 읽어 bytesReceived 신호로 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void TcpLink::onReadyRead()
{
    const QByteArray data = _socket->readAll();
    if (!data.isEmpty())
        emit bytesReceived(data);
}


// ─────────────────────────────────────────────────────────────────────────────
// onErrorOccurred()
// 소켓 에러 발생 시 linkError 신호를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void TcpLink::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit linkError(QString("TCP error: %1").arg(_socket->errorString()));
}
