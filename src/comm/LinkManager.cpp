#include "LinkManager.h"
#include "util/log/logger.h"

LinkManager::LinkManager(QObject* parent) : QObject(parent) {}

LinkManager::~LinkManager()
{
    removeLink();
}

bool LinkManager::setLink(std::unique_ptr<ILink> link)
{
    removeLink();
    _link = std::move(link);

    connect(_link.get(), &ILink::bytesReceived,    this, &LinkManager::bytesReceived);
    connect(_link.get(), &ILink::linkConnected,    this, &LinkManager::linkConnected);
    connect(_link.get(), &ILink::linkDisconnected, this, &LinkManager::linkDisconnected);
    connect(_link.get(), &ILink::linkError,        this, &LinkManager::linkError);

    return _link->connectLink();
}

void LinkManager::removeLink()
{
    if (_link) {
        _link->disconnectLink();
        _link.reset();
    }
}

bool LinkManager::isConnected() const
{
    return _link && _link->isConnected();
}

bool LinkManager::sendBytes(const QByteArray& data)
{
    if (!_link) return false;
    return _link->sendBytes(data);
}
