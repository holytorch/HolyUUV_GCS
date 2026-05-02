#pragma once

#include <QObject>
#include <memory>
#include "ILink.h"

class LinkManager : public QObject {
    //qt의 모든 클래스 조상
    Q_OBJECT
public:
    //생성자 소멸자 적어놓음
    explicit LinkManager(QObject* parent = nullptr);
    ~LinkManager() override;

    bool setLink(std::unique_ptr<ILink> link);
    void removeLink();

    bool    isConnected() const;
    ILink*  activeLink() const { return _link.get(); }
    bool    sendBytes(const QByteArray& data);

signals:
    void bytesReceived(const QByteArray& data);
    void linkConnected();
    void linkDisconnected();
    void linkError(const QString& message);

private:
    // 멤버 변수 (Stack Memory에 미리 할당됨)
    std::unique_ptr<ILink> _link;
};
