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
// 소멸 시 활성 링크를 안전하게 해제한다.
// ─────────────────────────────────────────────────────────────────────────────
LinkManager::~LinkManager()
{
    removeLink();
    qInfo("[exit] LinkManager");
}


// ─────────────────────────────────────────────────────────────────────────────
// setLink()
// 이전 링크가 있으면 먼저 해제한 뒤 새 링크의 소유권을 넘겨받는다.
// ILink의 시그널을 LinkManager 시그널로 중계하도록 연결하고 connectLink()를 호출한다.
// 반환값: connectLink() 결과 (포트 열기·소켓 바인딩 성공 여부).
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
// 현재 링크를 연결 해제하고 삭제한다.
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
// 활성 링크가 있고 연결된 상태이면 true를 반환한다.
// ─────────────────────────────────────────────────────────────────────────────
bool LinkManager::isConnected() const
{
    return _link && _link->isConnected();
}


// ─────────────────────────────────────────────────────────────────────────────
// sendBytes()
// 활성 링크로 데이터를 전송한다. 링크가 없으면 false를 반환한다.
// ─────────────────────────────────────────────────────────────────────────────
bool LinkManager::sendBytes(const QByteArray& data)
{
    if (!_link) return false;
    return _link->sendBytes(data);
}
