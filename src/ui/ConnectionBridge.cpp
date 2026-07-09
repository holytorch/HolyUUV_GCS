#include "ConnectionBridge.h"

#include <QDebug>


ConnectionBridge::ConnectionBridge(QObject* parent) : QObject(parent)
{
    qInfo("[init] ConnectionBridge");
}


ConnectionBridge::~ConnectionBridge()
{
    qInfo("[exit] ConnectionBridge");
}


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
    // A newly detected vehicle starts at maximum signal strength (a HEARTBEAT was
    // received, so the link is OK).
    _slots[sysid].signalLevel = 3;
    emit detectedSysidsChanged();
    emit vehiclesInfoChanged();
}


void ConnectionBridge::removeDetectedSysid(int sysid)
{
    const int before = _detectedSysids.size();
    _detectedSysids.removeAll(QVariant(sysid));
    const bool slotRemoved = (_slots.remove(sysid) > 0);
    if (_detectedSysids.size() != before || slotRemoved) {
        emit detectedSysidsChanged();
        emit vehiclesInfoChanged();
    }
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


// Called on SYS_STATUS from any sysid (no active filter). Stores it in the slot.
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
