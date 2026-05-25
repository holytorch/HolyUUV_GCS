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
    emit detectedSysidsChanged();
}


void ConnectionBridge::clearDetectedSysids()
{
    if (_detectedSysids.isEmpty()) return;
    _detectedSysids.clear();
    emit detectedSysidsChanged();
}


void ConnectionBridge::setActiveSysidMirror(int sysid)
{
    if (_activeSysid == sysid) return;
    _activeSysid = sysid;
    emit activeSysidChanged();
}
