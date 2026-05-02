#include "LinkManager.h"
#include "util/log/logger.h"

LinkManager::LinkManager(QObject* parent) : QObject(parent) {}

LinkManager::~LinkManager()
{
    removeLink();
}

bool LinkManager::setLink(std::unique_ptr<ILink> link)
{
    // 기존 링크가 있으면 먼저 끊고 제거
    removeLink();

    // 소유권 이전: Application이 만든 SerialLink/UdpLink를 _link가 넘겨받음
    // unique_ptr은 복사 불가 → move로 소유권만 이동 (이후 link는 null)
    _link = std::move(link);

    // ILink 시그널 → LinkManager 시그널로 중계
    // SerialLink/UdpLink 내부 이벤트를 외부(Application)에서 받을 수 있게 연결
    connect(_link.get(), &ILink::bytesReceived,    this, &LinkManager::bytesReceived);
    connect(_link.get(), &ILink::linkConnected,    this, &LinkManager::linkConnected);
    connect(_link.get(), &ILink::linkDisconnected, this, &LinkManager::linkDisconnected);
    connect(_link.get(), &ILink::linkError,        this, &LinkManager::linkError);

    // 실제 연결 시도 후 성공 여부 반환 (Serial이면 포트 열기, UDP면 소켓 바인딩)
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
