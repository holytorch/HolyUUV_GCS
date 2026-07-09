#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include "vehicle/VehicleState.h"

// ─────────────────────────────────────────────────────────────────────────────
// VehicleManager
// The single source of truth for multi-robot operation. It owns one VehicleState
// per sysid (QObject parent-child → automatically cleaned up when the manager is
// destroyed, RAII). A vehicle is created on first detection and removed on its
// per-vehicle timeout / disconnect.
//
// Two concepts are kept separate:
//   - tracked : every detected vehicle. Each is tracked and rendered with its own
//               full telemetry.
//   - active  : the single vehicle the user has selected. Target of the CONTROL
//               CENTER / joystick / commands.
//
// It also exposes the vehicle list to QML as a QAbstractListModel (roles: vehicle,
// sysid). QML delegates use the existing VehicleState properties directly, e.g.
// model.vehicle.latitude.
// ─────────────────────────────────────────────────────────────────────────────
class VehicleManager : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(VehicleState* activeVehicle READ activeVehicle NOTIFY activeVehicleChanged)
    Q_PROPERTY(int           activeSysid   READ activeSysid    WRITE setActiveSysid
                                           NOTIFY activeVehicleChanged)
    Q_PROPERTY(int           count         READ count          NOTIFY countChanged)

public:
    enum Roles {
        VehicleRole = Qt::UserRole + 1,   // VehicleState* (QObject)
        SysidRole,                        // int
    };

    explicit VehicleManager(QObject* parent = nullptr);

    // Ownership: creates and returns the vehicle if absent; returns the existing
    // one otherwise. (parent=this → automatic cleanup)
    VehicleState* getOrCreate(int sysid);
    VehicleState* vehicle(int sysid) const { return _vehicles.value(sysid, nullptr); }
    void          removeVehicle(int sysid);
    void          clear();

    VehicleState* activeVehicle() const { return _vehicles.value(_activeSysid, nullptr); }
    int           activeSysid()   const { return _activeSysid; }
    Q_INVOKABLE void setActiveSysid(int sysid);

    int count() const { return _order.size(); }

    // QAbstractListModel
    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void vehicleAdded(int sysid);
    void vehicleRemoved(int sysid);
    void activeVehicleChanged();
    void countChanged();

private:
    // Until the user clicks a card, the lowest sysid (usually 1) is kept as the
    // default active vehicle.
    void _applyDefaultActive();

    QHash<int, VehicleState*> _vehicles;   // owned (QObject children)
    QList<int>                _order;       // stable row order (prevents dangling delegates)
    int                       _activeSysid = 0;
    bool                      _userPicked  = false;   // whether the user picked the active vehicle
};
