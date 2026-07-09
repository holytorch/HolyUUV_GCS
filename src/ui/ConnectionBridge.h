#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// ConnectionBridge
// A QML ↔ LinkManager + MavlinkManager transmit/receive bridge.
//   - QML requests a UDP connection by host/port → Application forwards it to LinkManager
//   - LinkManager state → reflected in this bridge's connected property
//   - Mirrors the sysid list detected by MavlinkManager + the active sysid to QML
//   - QML selects a sysid → forwarded to MavlinkManager::setActiveSysid
//
// Phase 1: only one connection is active at a time (LinkManager single-link).
// The next connectUdp call automatically tears down the existing connection first.
// ─────────────────────────────────────────────────────────────────────────────
class ConnectionBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool         connected       READ connected       NOTIFY connectedChanged)
    Q_PROPERTY(QString      currentHost     READ currentHost     NOTIFY currentEndpointChanged)
    Q_PROPERTY(int          currentPort     READ currentPort     NOTIFY currentEndpointChanged)
    Q_PROPERTY(QVariantList  detectedSysids READ detectedSysids  NOTIFY detectedSysidsChanged)
    Q_PROPERTY(int          activeSysid     READ activeSysid     NOTIFY activeSysidChanged)
    // Per-sysid telemetry slots (for the cards) — independent of the active vehicle,
    // covering every detected vehicle. Each entry: { sysid, batteryRemaining, voltage, signalLevel }
    Q_PROPERTY(QVariantList  vehiclesInfo   READ vehiclesInfo    NOTIFY vehiclesInfoChanged)
public:
    explicit ConnectionBridge(QObject* parent = nullptr);
    ~ConnectionBridge() override;

    bool         connected()       const { return _connected; }
    QString      currentHost()     const { return _host; }
    int          currentPort()     const { return _port; }
    QVariantList detectedSysids()  const { return _detectedSysids; }
    int          activeSysid()     const { return _activeSysid; }
    QVariantList vehiclesInfo()    const;

    Q_INVOKABLE void connectUdp(const QString& host, int port);
    Q_INVOKABLE void disconnectLink();
    // Called when QML clicks a sysid in the tree. Forwarded to MavlinkManager to
    // change the active vehicle.
    Q_INVOKABLE void setActiveSysid(int sysid);

    // Application side mirrors LinkManager/MavlinkManager state changes
    void setConnected(bool c);
    void addDetectedSysid(int sysid);
    void removeDetectedSysid(int sysid);   // remove a timed-out vehicle's card
    void clearDetectedSysids();
    void setActiveSysidMirror(int sysid);   // emit only (MavlinkManager applies the actual change)

public slots:
    // Update from any sysid's SYS_STATUS (called by MavlinkManager). Updates the slot
    // then emits vehiclesInfoChanged.
    void onAnyVehicleSysStatus(int sysid, int batteryRemaining, float voltage);

signals:
    void connectedChanged();
    void currentEndpointChanged();
    void detectedSysidsChanged();
    void activeSysidChanged();
    void vehiclesInfoChanged();

    void connectRequested(const QString& host, quint16 port);
    void disconnectRequested();
    void activeSysidChangeRequested(int sysid);

private:
    struct VehicleSlot {
        int   batteryRemaining = -1;
        float voltage          = 0.0f;
        int   signalLevel      = 0;   // 0=no signal, 3=full
    };

    bool         _connected = false;
    QString      _host;
    int          _port = 0;
    QVariantList _detectedSysids;
    int          _activeSysid = 0;
    QMap<int, VehicleSlot> _slots;
};
