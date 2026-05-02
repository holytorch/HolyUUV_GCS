#pragma once

#include "ILink.h"
#include <QSerialPort>

struct SerialConfig {
    QString portName  = "/dev/ttyUSB0";
    int     baudRate  = 57600;
};

class SerialLink : public ILink {
    Q_OBJECT
public:
    explicit SerialLink(const SerialConfig& config, QObject* parent = nullptr);
    ~SerialLink() override;

    bool    connectLink() override;
    void    disconnectLink() override;
    bool    isConnected() const override;
    bool    sendBytes(const QByteArray& data) override;
    QString linkName() const override;

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    SerialConfig  _config;
    QSerialPort*  _serial = nullptr;
};
