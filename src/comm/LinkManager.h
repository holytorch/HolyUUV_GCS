#pragma once

#include <QObject>
#include <memory>
#include "ILink.h"

// ─────────────────────────────────────────────────────────────────────────────
// LinkManager
// 단일 활성 링크(UdpLink)를 소유하고 관리한다.
// setLink()로 링크를 교체하면 이전 링크를 안전하게 해제한 뒤 새 링크를 연결한다.
// ILink의 시그널을 그대로 재발신해 외부(Application)가 동일한 시그널을 사용할 수 있게 한다.
// ─────────────────────────────────────────────────────────────────────────────
class LinkManager : public QObject {
    Q_OBJECT
public:
    explicit LinkManager(QObject* parent = nullptr);
    ~LinkManager() override;

    bool setLink(std::unique_ptr<ILink> link);
    void removeLink();

    bool    isConnected()                       const;
    ILink*  activeLink()                        const { return _link.get(); }
    bool    sendBytes(const QByteArray& data);

signals:
    void bytesReceived(const QByteArray& data);
    void linkConnected();
    void linkDisconnected();
    void linkError(const QString& message);

private:
    std::unique_ptr<ILink> _link;
};
