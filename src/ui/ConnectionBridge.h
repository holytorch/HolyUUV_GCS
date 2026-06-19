#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// ConnectionBridge
// QML ↔ LinkManager + MavlinkManager 송수신 브리지.
//   - QML이 호스트/포트로 UDP 연결 요청 → Application이 LinkManager에 전달
//   - LinkManager 상태 → 이 브리지의 connected 프로퍼티에 반영
//   - MavlinkManager가 감지한 sysid 목록 + 활성 sysid를 mirror해 QML에 노출
//   - QML이 sysid 선택 → MavlinkManager::setActiveSysid로 전달
//
// Phase 1: 한 번에 한 연결만 활성 (LinkManager single-link).
// 다음 connectUdp 호출 시 기존 연결은 자동 해제 후 새로 연결.
// ─────────────────────────────────────────────────────────────────────────────
class ConnectionBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool         connected       READ connected       NOTIFY connectedChanged)
    Q_PROPERTY(QString      currentHost     READ currentHost     NOTIFY currentEndpointChanged)
    Q_PROPERTY(int          currentPort     READ currentPort     NOTIFY currentEndpointChanged)
    Q_PROPERTY(QVariantList  detectedSysids READ detectedSysids  NOTIFY detectedSysidsChanged)
    Q_PROPERTY(int          activeSysid     READ activeSysid     NOTIFY activeSysidChanged)
    // sysid별 텔레메트리 슬롯 (카드 표시용) — 활성 차량 무관, 모든 감지된 차량의 정보
    // 각 항목: { sysid, batteryRemaining, voltage, signalLevel }
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
    // QML이 트리에서 sysid 클릭 시 호출. MavlinkManager로 전달돼 active 변경.
    Q_INVOKABLE void setActiveSysid(int sysid);

    // Application 측에서 LinkManager/MavlinkManager 상태 변화를 미러
    void setConnected(bool c);
    void addDetectedSysid(int sysid);
    void removeDetectedSysid(int sysid);   // 타임아웃된 차량 카드 제거
    void clearDetectedSysids();
    void setActiveSysidMirror(int sysid);   // emit만 (실제 변경은 MavlinkManager가)

public slots:
    // 모든 sysid의 SYS_STATUS 업데이트 (MavlinkManager가 호출). 슬롯 갱신 후 vehiclesInfoChanged emit.
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
