#pragma once

#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// CompassWidget
// 원형 컴파스 위젯. QGC의 QGCCompassWidget을 QPainter로 재구현.
//
// - 헤딩에 따라 컴파스 카드가 회전
// - N/NE/E/SE/S/SW/W/NW 방위 레이블 및 눈금 표시
// - 상단 고정 빨간 삼각 마커가 현재 헤딩 방향을 가리킴
// - 중앙에 헤딩 수치 표시
// ─────────────────────────────────────────────────────────────────────────────
class CompassWidget : public QWidget {
    Q_OBJECT
public:
    explicit CompassWidget(QWidget* parent = nullptr);

    void setHeading(float headingDeg);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    float _headingDeg = 0.0f;
};
