#pragma once

#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// AttitudeIndicator
// 인공 수평선(Artificial Horizon). QGC의 QGCArtificialHorizon + QGCPitchIndicator를
// QPainter로 재구현한 위젯.
//
// - 하늘(파란 그라디언트) / 지면(갈색 그라디언트)을 롤 각도로 회전
// - 피치 각도에 따라 수평선이 위아래로 이동
// - 피치 래더(5° 간격), 롤 눈금 호, 고정 노란색 크로스헤어 표시
// ─────────────────────────────────────────────────────────────────────────────
class AttitudeIndicator : public QWidget {
    Q_OBJECT
public:
    explicit AttitudeIndicator(QWidget* parent = nullptr);

    void setAttitude(float rollDeg, float pitchDeg);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    float _rollDeg  = 0.0f;
    float _pitchDeg = 0.0f;
};
