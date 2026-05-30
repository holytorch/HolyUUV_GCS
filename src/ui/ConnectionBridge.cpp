#include "ConnectionBridge.h"

#include <QDebug>


void ConnectionBridge::connectUdp(const QString& host, int port)
{
    qInfo("ConnectionBridge: connect requested %s:%d",
          qPrintable(host), port);

    _host = host;
    _port = port;
    emit currentEndpointChanged();
    emit connectRequested(host, static_cast<quint16>(port));
}


void ConnectionBridge::disconnectLink()
{
    qInfo("ConnectionBridge: disconnect requested");
    emit disconnectRequested();
}


void ConnectionBridge::setConnected(bool c)
{
    if (_connected == c) return;
    _connected = c;
    emit connectedChanged();
}


void ConnectionBridge::setActiveSysid(int sysid)
{
    qInfo("ConnectionBridge: setActiveSysid(%d) requested", sysid);
    emit activeSysidChangeRequested(sysid);
}


void ConnectionBridge::addDetectedSysid(int sysid)
{
    if (_detectedSysids.contains(sysid)) return;
    _detectedSysids.append(sysid);
    // 감지된 차량은 통신 강도 max로 시작 (HEARTBEAT 받았으니 연결 OK)
    _slots[sysid].signalLevel = 3;
    emit detectedSysidsChanged();
    emit vehiclesInfoChanged();
}


void ConnectionBridge::clearDetectedSysids()
{
    if (_detectedSysids.isEmpty() && _slots.isEmpty()) return;
    _detectedSysids.clear();
    _slots.clear();
    emit detectedSysidsChanged();
    emit vehiclesInfoChanged();
}


void ConnectionBridge::setActiveSysidMirror(int sysid)
{
    if (_activeSysid == sysid) return;
    _activeSysid = sysid;
    emit activeSysidChanged();
}


// 모든 sysid의 SYS_STATUS 수신 시 호출 (활성 필터 없음). 슬롯에 저장.
void ConnectionBridge::onAnyVehicleSysStatus(int sysid, int batteryRemaining, float voltage)
{
    auto& slot = _slots[sysid];
    if (slot.batteryRemaining == batteryRemaining &&
        qFuzzyCompare(slot.voltage + 1.0f, voltage + 1.0f))
        return;
    slot.batteryRemaining = batteryRemaining;
    slot.voltage = voltage;
    emit vehiclesInfoChanged();
}


QVariantList ConnectionBridge::vehiclesInfo() const
{
    QVariantList list;
    list.reserve(_detectedSysids.size());
    for (const auto& v : _detectedSysids) {
        const int s = v.toInt();
        const auto& slot = _slots.value(s);
        QVariantMap m;
        m[QStringLiteral("sysid")]            = s;
        m[QStringLiteral("batteryRemaining")] = slot.batteryRemaining;
        m[QStringLiteral("voltage")]          = slot.voltage;
        m[QStringLiteral("signalLevel")]      = slot.signalLevel;
        list.append(m);
    }
    return list;
}
