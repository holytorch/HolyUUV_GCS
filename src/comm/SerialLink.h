#pragma once

#include "ILink.h"
#include <QSerialPort>

// ─────────────────────────────────────────────────────────────────────────────
// SerialConfig
// SerialLink 생성에 필요한 포트 이름과 보드레이트.
// ─────────────────────────────────────────────────────────────────────────────
struct SerialConfig {
    QString portName  = "/dev/ttyUSB0";
    int     baudRate  = 57600;
};

// ─────────────────────────────────────────────────────────────────────────────
// SerialLink
// QSerialPort 기반 ILink 구현체.
// MAVLink 보드가 VID/PID로 감지된 경우 Application이 이 클래스를 생성한다.
// 데이터 수신은 readyRead 시그널로 처리되며 에러는 errorOccurred로 처리된다.
// ─────────────────────────────────────────────────────────────────────────────
class SerialLink : public ILink {
    Q_OBJECT
public:
    explicit SerialLink(const SerialConfig& config, QObject* parent = nullptr);
    ~SerialLink() override;

    bool    connectLink()                       override;
    void    disconnectLink()                    override;
    bool    isConnected()               const   override;
    bool    sendBytes(const QByteArray& data)   override;
    QString linkName()                  const   override;

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    SerialConfig  _config;
    QSerialPort*  _serial = nullptr;
};
