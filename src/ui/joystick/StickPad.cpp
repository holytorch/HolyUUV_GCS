#include "StickPad.h"

#include <QPainter>
#include <QMouseEvent>
#include <QRadialGradient>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// StickPad()
// ─────────────────────────────────────────────────────────────────────────────
StickPad::StickPad(QWidget* parent) : QWidget(parent)
{
    // mouseMoveEvent는 버튼 눌린 동안만 자동 발생 — setMouseTracking 불필요.
    setCursor(Qt::CrossCursor);
}


// ─────────────────────────────────────────────────────────────────────────────
// center()
// 노브를 중립으로 이동하고 valueChanged(0, 0)을 발신한다.
// ─────────────────────────────────────────────────────────────────────────────
void StickPad::center()
{
    _x = 0.0f;
    _y = 0.0f;
    update();
    emit valueChanged(0.0f, 0.0f);
}


// ─────────────────────────────────────────────────────────────────────────────
// paintEvent()
// 패드 배경(원형 그라디언트) → 십자선 → 중앙 링 → 노브 순서로 그린다.
// HTML의 --knob 그라디언트(#dcf8ff → #66d0ff)와 패드 스타일을 Qt로 재현한다.
// ─────────────────────────────────────────────────────────────────────────────
void StickPad::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal w = width();
    const qreal h = height();
    const qreal r = qMin(w, h) / 2.0 - 2.0;
    const QPointF c(w / 2.0, h / 2.0);

    // Pad background
    QRadialGradient bg(c, r);
    bg.setColorAt(0.0, QColor(97, 211, 255, 20));
    bg.setColorAt(0.5, QColor(255, 255, 255, 8));
    bg.setColorAt(1.0, QColor(255, 255, 255, 15));
    p.setBrush(bg);
    p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
    p.drawEllipse(c, r, r);

    // Crosshair lines
    p.setPen(QPen(QColor(255, 255, 255, 30), 1.0));
    p.drawLine(QPointF(c.x() - r * 0.88, c.y()), QPointF(c.x() + r * 0.88, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - r * 0.88), QPointF(c.x(), c.y() + r * 0.88));

    // Center ring (corresponds to HTML's radial-gradient ring at 46% radius)
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 20), 1.0));
    p.drawEllipse(c, r * 0.46, r * 0.46);

    // Knob (clamped to pad circle)
    const qreal knobR    = r * 0.36;
    const qreal maxTravel = r - knobR - 1.0;
    const QPointF knobC(c.x() + _x * maxTravel, c.y() + _y * maxTravel);

    QRadialGradient knob(knobC + QPointF(-knobR * 0.25, -knobR * 0.3), knobR * 1.6);
    knob.setColorAt(0.0, QColor(220, 248, 255));
    knob.setColorAt(1.0, QColor(102, 208, 255));
    p.setBrush(knob);
    p.setPen(QPen(QColor(255, 255, 255, 80), 1.0));
    p.drawEllipse(knobC, knobR, knobR);
}


// ─────────────────────────────────────────────────────────────────────────────
// mousePressEvent() / mouseMoveEvent() / mouseReleaseEvent()
// 드래그 시작·갱신·종료. 종료 시 center()를 호출해 노브를 자동 복귀한다.
// ─────────────────────────────────────────────────────────────────────────────
void StickPad::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        _dragging = true;
        updateFromPos(e->pos());
    }
}

void StickPad::mouseMoveEvent(QMouseEvent* e)
{
    if (_dragging)
        updateFromPos(e->pos());
}

void StickPad::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && _dragging) {
        _dragging = false;
        center();
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// updateFromPos()
// 마우스 위치를 [-1, 1] 정규화 좌표로 변환하고 valueChanged를 발신한다.
// 패드 반지름 밖으로 나가면 단위원 가장자리로 클램프한다.
// ─────────────────────────────────────────────────────────────────────────────
void StickPad::updateFromPos(const QPoint& pos)
{
    const float cx = width()  / 2.0f;
    const float cy = height() / 2.0f;
    const float r  = qMin(width(), height()) / 2.0f - 2.0f;

    float dx   = pos.x() - cx;
    float dy   = pos.y() - cy;
    const float dist = std::hypot(dx, dy);

    if (dist > r && dist > 0.0f) {
        dx *= r / dist;
        dy *= r / dist;
    }

    _x = clamp(dx / r);
    _y = clamp(dy / r);
    update();
    emit valueChanged(_x, _y);
}
