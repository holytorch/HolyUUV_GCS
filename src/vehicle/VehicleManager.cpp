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

    // 사용자가 아직 안 골랐으면 가장 낮은 sysid(보통 1번)를 기본 활성으로 → 맵도 그쪽 중심.
    _applyDefaultActive();

    return state;
}


// 사용자가 카드를 누르기 전까지, 현재 존재하는 가장 낮은 sysid를 활성으로 유지.
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
        state->deleteLater();   // 이벤트 루프 안전 삭제 (delegate 참조 정리 후)

    emit vehicleRemoved(sysid);
    emit countChanged();

    // 활성 차량이 사라졌으면 남은 것 중 가장 낮은 sysid로(없으면 0). 사용자 선택 플래그는 해제.
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

    const QList<int> removed = _order;   // 마커 정리 통지용

    beginResetModel();
    const auto states = _vehicles.values();
    _vehicles.clear();
    _order.clear();
    endResetModel();

    for (VehicleState* s : states)
        if (s) s->deleteLater();

    // 각 차량 제거 통지 (TerrainScene 3D 마커 등 외부 정리용)
    for (int sysid : removed)
        emit vehicleRemoved(sysid);

    emit countChanged();
    // 연결 해제 → 사용자 선택 해제, 활성 0 (다음 연결 때 다시 기본=최저 sysid)
    _userPicked  = false;
    if (_activeSysid != 0) {
        _activeSysid = 0;
        emit activeVehicleChanged();
    }
}


// 사용자(카드 클릭)가 활성 차량을 고른다. 이후 자동 기본값이 덮어쓰지 않는다.
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
