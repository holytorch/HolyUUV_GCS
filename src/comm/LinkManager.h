#pragma once

#include <QObject>
#include <memory>
#include "ILink.h"

// ─────────────────────────────────────────────────────────────────────────────
// LinkManager
// Owns and manages a single active link (UdpLink). Swapping the link via
// setLink() safely tears down the previous one before connecting the new one.
// It re-emits the ILink signals verbatim, so callers (Application) can subscribe
// to the same set of signals regardless of the underlying link.
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
