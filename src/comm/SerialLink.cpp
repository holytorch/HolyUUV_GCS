#include "SerialLink.h"
#include "util/log/logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// SerialLink()
// QSerialPort 객체를 this의 자식으로 생성한다.
// ─────────────────────────────────────────────────────────────────────────────
SerialLink::SerialLink(const SerialConfig& config, QObject* parent)
    : ILink(parent), _config(config)
{
    _serial = new QSerialPort(this);
}


// ─────────────────────────────────────────────────────────────────────────────
// ~SerialLink()
// 소멸 시 포트를 닫는다.
// ─────────────────────────────────────────────────────────────────────────────
SerialLink::~SerialLink()
{
    disconnectLink();
}


// ─────────────────────────────────────────────────────────────────────────────
// connectLink()
// 설정값으로 시리얼 포트를 열고 readyRead / errorOccurred 시그널을 연결한다.
// 성공 시 linkConnected를 발신하고 true를 반환한다.
// ─────────────────────────────────────────────────────────────────────────────
bool SerialLink::connectLink()
{
    _serial->setPortName(_config.portName);
    _serial->setBaudRate(_config.baudRate);
    _serial->setDataBits(QSerialPort::Data8);
    _serial->setParity(QSerialPort::NoParity);
    _serial->setStopBits(QSerialPort::OneStop);
    _serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!_serial->open(QIODevice::ReadWrite)) {
        emit linkError(QString("Failed to open serial port %1: %2")
            .arg(_config.portName, _serial->errorString()));
        return false;
    }

    connect(_serial, &QSerialPort::readyRead,     this, &SerialLink::onReadyRead);
    connect(_serial, &QSerialPort::errorOccurred, this, &SerialLink::onErrorOccurred);

    emit linkConnected();
    LOG_INFO("Serial connected: %s @ %d baud",
        qPrintable(_config.portName), _config.baudRate);
    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// disconnectLink()
// 포트가 열려 있으면 닫고 linkDisconnected를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void SerialLink::disconnectLink()
{
    if (_serial && _serial->isOpen()) {
        _serial->close();
        emit linkDisconnected();
        LOG_INFO("Serial disconnected: %s", qPrintable(_config.portName));
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// isConnected()
// ─────────────────────────────────────────────────────────────────────────────
bool SerialLink::isConnected() const
{
    return _serial && _serial->isOpen();
}


// ─────────────────────────────────────────────────────────────────────────────
// sendBytes()
// 데이터를 시리얼 포트에 쓴다. 전송 바이트 수가 일치하면 true를 반환한다.
// ─────────────────────────────────────────────────────────────────────────────
bool SerialLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;
    return _serial->write(data) == data.size();
}


// ─────────────────────────────────────────────────────────────────────────────
// linkName()
// ─────────────────────────────────────────────────────────────────────────────
QString SerialLink::linkName() const
{
    return QString("Serial[%1]").arg(_config.portName);
}


// ─────────────────────────────────────────────────────────────────────────────
// onReadyRead()
// 수신 버퍼의 모든 데이터를 읽어 bytesReceived 신호로 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void SerialLink::onReadyRead()
{
    const QByteArray data = _serial->readAll();
    if (!data.isEmpty())
        emit bytesReceived(data);
}


// ─────────────────────────────────────────────────────────────────────────────
// onErrorOccurred()
// NoError 이외의 에러 발생 시 linkError 신호를 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void SerialLink::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError)
        emit linkError(QString("Serial error: %1").arg(_serial->errorString()));
}
