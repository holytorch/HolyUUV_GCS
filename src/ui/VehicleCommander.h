#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// VehicleCommander
// QML → MAVLink 송신 브리지. QML(또는 다른 C++ 코드)이 Q_INVOKABLE 메서드로
// arm/disarm/setMode를 요청하면, 시그널로 전파한다.
// Application이 이 시그널을 MavlinkManager의 send* 슬롯에 연결한다.
//
// 송신만 책임. 현재 상태(armed 여부, 현재 mode)는 VehicleState를 보면 됨.
// ─────────────────────────────────────────────────────────────────────────────
class VehicleCommander : public QObject {
    Q_OBJECT
public:
    explicit VehicleCommander(QObject* parent = nullptr);
    ~VehicleCommander() override;

    Q_INVOKABLE void setArm(bool arm);
    // ArduSub 모드 이름. 인식하는 값: STABILIZE, ACRO, ALT_HOLD, AUTO, GUIDED,
    // CIRCLE, SURFACE, POSHOLD, MANUAL, MOTOR_DETECT (대소문자 무관).
    Q_INVOKABLE void setMode(const QString& modeName);
    // QML 조이스틱 → MAVLink MANUAL_CONTROL. 50Hz로 호출 권장.
    // x/y/r: [-1000, 1000], z: [0, 1000] (500=중립). buttons는 비트마스크.
    Q_INVOKABLE void sendManualControl(int x, int y, int z, int r, int buttons);

signals:
    void armRequested(bool arm);
    void setModeRequested(uint32_t customMode);
    void manualControlRequested(int16_t x, int16_t y, int16_t z, int16_t r, uint16_t buttons);
};
