#pragma once

#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// DepthGauge
// 수직 바 형태의 수심 게이지.
//
// - 0 ~ maxDepth(m) 범위를 파란 그라디언트 바로 표시
// - 왼쪽에 눈금, 하단에 현재 수심 수치 표시
// ─────────────────────────────────────────────────────────────────────────────
class DepthGauge : public QWidget {
    Q_OBJECT
public:
    explicit DepthGauge(QWidget* parent = nullptr);

    void setDepth(float depthM);
    void setMaxDepth(float maxDepthM);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    float _depth    = 0.0f;
    float _maxDepth = 100.0f;
};
