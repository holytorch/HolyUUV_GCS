#include "SerialLink.h"
#include "util/log/logger.h"

SerialLink::SerialLink(const SerialConfig& config, QObject* parent)
    : ILink(parent), _config(config)
{
    _serial = new QSerialPort(this);
}

SerialLink::~SerialLink()
{
    disconnectLink();
}

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

    connect(_serial, &QSerialPort::readyRead,       this, &SerialLink::onReadyRead);
    connect(_serial, &QSerialPort::errorOccurred,   this, &SerialLink::onErrorOccurred);

    emit linkConnected();
    LOG_INFO("Serial connected: %s @ %d baud",
        qPrintable(_config.portName), _config.baudRate);
    return true;
}

void SerialLink::disconnectLink()
{
    if (_serial && _serial->isOpen()) {
        _serial->close();
        emit linkDisconnected();
        LOG_INFO("Serial disconnected: %s", qPrintable(_config.portName));
    }
}

bool SerialLink::isConnected() const
{
    return _serial && _serial->isOpen();
}

bool SerialLink::sendBytes(const QByteArray& data)
{
    if (!isConnected()) return false;
    return _serial->write(data) == data.size();
}

QString SerialLink::linkName() const
{
    return QString("Serial[%1]").arg(_config.portName);
}

void SerialLink::onReadyRead()
{
    const QByteArray data = _serial->readAll();
    if (!data.isEmpty()) {
        emit bytesReceived(data);
    }
}

void SerialLink::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        emit linkError(QString("Serial error: %1").arg(_serial->errorString()));
    }
}
