#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include "vehicle/VehicleState.h"

// ─────────────────────────────────────────────────────────────────────────────
// VehicleManager
// 다중로봇의 단일 진실원천. sysid별로 VehicleState 하나씩을 소유한다
// (QObject parent-child → 매니저 소멸 시 자동 정리, RAII). 차량은 첫 감지 때
// 생성되고 per-vehicle 타임아웃/연결 해제 때 제거된다.
//
// 두 개념을 분리한다:
//   - tracked : 감지된 모든 차량. 각자 완전한 텔레메트리로 추적·렌더된다.
//   - active  : 사용자가 고른 한 대. CONTROL CENTER/조이스틱/명령 대상.
//
// 동시에 QAbstractListModel로서 QML에 차량 목록을 노출한다 (role: vehicle, sysid).
// QML 델리게이트는 model.vehicle.latitude 처럼 기존 VehicleState 프로퍼티를 그대로 쓴다.
// (QML 노출은 마이그레이션 후반 단계에서 연결 — 지금은 모델만 준비)
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

    // 소유: 없으면 생성해 반환. 존재하면 그대로 반환. (parent=this → 자동 정리)
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
    // 사용자가 카드를 누르기 전까지는 가장 낮은 sysid(보통 1번)를 기본 활성으로 둔다.
    void _applyDefaultActive();

    QHash<int, VehicleState*> _vehicles;   // 소유 (QObject child)
    QList<int>                _order;       // 행 순서 안정화 (delegate dangling 방지)
    int                       _activeSysid = 0;
    bool                      _userPicked  = false;   // 사용자가 직접 활성 차량을 골랐는지
};
