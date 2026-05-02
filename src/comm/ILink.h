#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class ILink : public QObject {
    Q_OBJECT
public:
    explicit ILink(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ILink() = default;

    virtual bool connectLink() = 0;
    virtual void disconnectLink() = 0;
    virtual bool isConnected() const = 0;
    virtual bool sendBytes(const QByteArray& data) = 0;
    virtual QString linkName() const = 0;

signals:
    void bytesReceived(const QByteArray& data);
    void linkConnected();
    void linkDisconnected();
    void linkError(const QString& message);
};
