#include "VehicleManager.h"

#include <QQmlEngine>   // setObjectOwnership — QML이 C++ 소유 객체를 GC하지 않도록

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

    // parent=this → 매니저 소멸 시 자동 해제 (RAII). 소유는 C++ 측에만 둔다.
    auto* state = new VehicleState(sysid, this);
    QQmlEngine::setObjectOwnership(state, QQmlEngine::CppOwnership);

    _vehicles.insert(sysid, state);
    _order.append(sysid);
    endInsertRows();

    emit vehicleAdded(sysid);
    emit countChanged();

    // 활성 차량이 아직 없으면 첫 차량을 기본 활성으로.
    if (_activeSysid == 0)
        setActiveSysid(sysid);

    return state;
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
        state->deleteLater();   // 이벤트 루프 안전 삭제 (delegate 참조 정리 후)

    emit vehicleRemoved(sysid);
    emit countChanged();

    // 활성 차량이 사라졌으면 남은 첫 차량으로(없으면 0).
    if (_activeSysid == sysid)
        setActiveSysid(_order.isEmpty() ? 0 : _order.first());
}


void VehicleManager::clear()
{
    if (_order.isEmpty()) {
        if (_activeSysid != 0) setActiveSysid(0);
        return;
    }

    beginResetModel();
    const auto states = _vehicles.values();
    _vehicles.clear();
    _order.clear();
    endResetModel();

    for (VehicleState* s : states)
        if (s) s->deleteLater();

    emit countChanged();
    setActiveSysid(0);
}


void VehicleManager::setActiveSysid(int sysid)
{
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
