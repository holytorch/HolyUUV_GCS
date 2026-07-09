#include "VehicleManager.h"

#include <QQmlEngine>   // setObjectOwnership — keep QML from garbage-collecting C++-owned objects

VehicleManager::VehicleManager(QObject* parent)
    : QAbstractListModel(parent)
{
}


VehicleState* VehicleManager::getOrCreate(int sysid)
{
    if (auto* v = _vehicles.value(sysid, nullptr))
        return v;

    const int row = _order.size();
    beginInsertRows(QModelIndex(), row, row);

    // parent=this → freed automatically when the manager is destroyed (RAII).
    // Ownership stays entirely on the C++ side.
    auto* state = new VehicleState(sysid, this);
    QQmlEngine::setObjectOwnership(state, QQmlEngine::CppOwnership);

    _vehicles.insert(sysid, state);
    _order.append(sysid);
    endInsertRows();

    emit vehicleAdded(sysid);
    emit countChanged();

    // If the user has not picked yet, keep the lowest sysid (usually 1) as the
    // default active → the map centers on it too.
    _applyDefaultActive();

    return state;
}


// Until the user clicks a card, keep the lowest currently existing sysid active.
void VehicleManager::_applyDefaultActive()
{
    if (_userPicked || _order.isEmpty()) return;
    int lowest = _order.first();
    for (int s : _order)
        if (s < lowest) lowest = s;
    if (lowest != _activeSysid) {
        _activeSysid = lowest;
        emit activeVehicleChanged();
    }
}


void VehicleManager::removeVehicle(int sysid)
{
    const int row = _order.indexOf(sysid);
    if (row < 0) return;

    beginRemoveRows(QModelIndex(), row, row);
    _order.removeAt(row);
    VehicleState* state = _vehicles.take(sysid);
    endRemoveRows();

    if (state)
        state->deleteLater();   // event-loop-safe deletion (after delegate refs are cleared)

    emit vehicleRemoved(sysid);
    emit countChanged();

    // If the active vehicle vanished, fall back to the lowest remaining sysid (0 if
    // none). The user-pick flag is cleared.
    if (_activeSysid == sysid) {
        int next = 0;
        if (!_order.isEmpty()) {
            next = _order.first();
            for (int s : _order) if (s < next) next = s;
        }
        _userPicked  = false;
        _activeSysid = next;
        emit activeVehicleChanged();
    }
}


void VehicleManager::clear()
{
    if (_order.isEmpty()) {
        _userPicked = false;
        if (_activeSysid != 0) { _activeSysid = 0; emit activeVehicleChanged(); }
        return;
    }

    const QList<int> removed = _order;   // for marker-cleanup notifications

    beginResetModel();
    const auto states = _vehicles.values();
    _vehicles.clear();
    _order.clear();
    endResetModel();

    for (VehicleState* s : states)
        if (s) s->deleteLater();

    // Notify removal of each vehicle (for external cleanup, e.g. TerrainScene 3D markers)
    for (int sysid : removed)
        emit vehicleRemoved(sysid);

    emit countChanged();
    // Disconnect → clear the user pick, active = 0 (next connect defaults back to
    // the lowest sysid).
    _userPicked  = false;
    if (_activeSysid != 0) {
        _activeSysid = 0;
        emit activeVehicleChanged();
    }
}


// The user (card click) selects the active vehicle. The automatic default no
// longer overrides it afterward.
void VehicleManager::setActiveSysid(int sysid)
{
    _userPicked = true;
    if (_activeSysid == sysid) return;
    _activeSysid = sysid;
    emit activeVehicleChanged();
}


int VehicleManager::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return _order.size();
}


QVariant VehicleManager::data(const QModelIndex& index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= _order.size()) return QVariant();

    const int sysid = _order.at(row);
    switch (role) {
        case VehicleRole:
            return QVariant::fromValue(static_cast<QObject*>(_vehicles.value(sysid, nullptr)));
        case SysidRole:
            return sysid;
        default:
            return QVariant();
    }
}


QHash<int, QByteArray> VehicleManager::roleNames() const
{
    return {
        { VehicleRole, QByteArrayLiteral("vehicle") },
        { SysidRole,   QByteArrayLiteral("sysid")   },
    };
}
