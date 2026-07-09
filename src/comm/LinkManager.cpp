#include "LinkManager.h"
#include "util/log/logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// LinkManager()
// ─────────────────────────────────────────────────────────────────────────────
LinkManager::LinkManager(QObject* parent) : QObject(parent)
{
    qInfo("[init] LinkManager");
}


// ─────────────────────────────────────────────────────────────────────────────
// ~LinkManager()
// Safely tears down the active link on destruction.
// ─────────────────────────────────────────────────────────────────────────────
LinkManager::~LinkManager()
{
    removeLink();
    qInfo("[exit] LinkManager");
}


// ─────────────────────────────────────────────────────────────────────────────
// setLink()
// Tears down any existing link first, then takes ownership of the new one.
// Wires the ILink signals to be relayed as LinkManager signals and calls
// connectLink().
// Returns: the result of connectLink() (whether opening the port / binding the
// socket succeeded).
// ─────────────────────────────────────────────────────────────────────────────
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


// ─────────────────────────────────────────────────────────────────────────────
// removeLink()
// Disconnects and deletes the current link.
// ─────────────────────────────────────────────────────────────────────────────
void LinkManager::removeLink()
{
    if (_link) {
        _link->disconnectLink();
        _link.reset();
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// isConnected()
// Returns true when an active link exists and is connected.
// ─────────────────────────────────────────────────────────────────────────────
bool LinkManager::isConnected() const
{
    return _link && _link->isConnected();
}


// ─────────────────────────────────────────────────────────────────────────────
// sendBytes()
// Sends data over the active link. Returns false when there is no link.
// ─────────────────────────────────────────────────────────────────────────────
bool LinkManager::sendBytes(const QByteArray& data)
{
    if (!_link) return false;
    return _link->sendBytes(data);
}
