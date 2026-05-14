#pragma once

#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// StickPad
// 원형 아날로그 스틱 패드. QPainter로 렌더링하며 마우스 드래그로 조작한다.
// 손을 떼면 자동으로 중립(0, 0)으로 복귀한다.
//
// 좌표계: valueX 오른쪽 = +1, valueY 아래쪽 = +1 (화면 좌표계)
// 범위: [-1, 1]
// ─────────────────────────────────────────────────────────────────────────────
class StickPad : public QWidget {
    Q_OBJECT
public:
    explicit StickPad(QWidget* parent = nullptr);

    float valueX() const { return _x; }
    float valueY() const { return _y; }

    void  center();

signals:
    void valueChanged(float x, float y);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    void  updateFromPos(const QPoint& pos);
    float clamp(float v) const { return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); }

    float _x        = 0.0f;
    float _y        = 0.0f;
    bool  _dragging = false;
};
