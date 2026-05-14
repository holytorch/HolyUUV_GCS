#include "CompassWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

CompassWidget::CompassWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(160, 160);
}

void CompassWidget::setHeading(float headingDeg)
{
    _headingDeg = headingDeg;
    update();
}

void CompassWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal cx = width()  / 2.0;
    const qreal cy = height() / 2.0;
    const qreal r  = qMin(cx, cy) - 3;

    // ── 배경 원 ───────────────────────────────────────────────
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(18, 20, 32));
    p.drawEllipse(QPointF(cx, cy), r, r);

    // ── 회전하는 컴파스 카드 ──────────────────────────────────
    p.save();
    p.translate(cx, cy);
    p.rotate(-_headingDeg);

    // 눈금 (5° 간격)
    for (int deg = 0; deg < 360; deg += 5) {
        qreal rad    = deg * M_PI / 180.0;
        qreal sinA   = std::sin(rad);
        qreal cosA   = -std::cos(rad);
        qreal tLen   = (deg % 45 == 0) ? 13 : (deg % 15 == 0) ? 8 : 5;
        qreal x1 = sinA * (r - tLen), y1 = cosA * (r - tLen);
        qreal x2 = sinA * r,          y2 = cosA * r;
        p.setPen(QPen(Qt::white, deg % 45 == 0 ? 2 : 1));
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // 방위 레이블
    static const char* labels[] = {"N","NE","E","SE","S","SW","W","NW"};
    for (int i = 0; i < 8; i++) {
        qreal deg  = i * 45.0;
        qreal rad  = deg * M_PI / 180.0;
        qreal lx   = std::sin(rad) * (r - 22);
        qreal ly   = -std::cos(rad) * (r - 22);
        bool isN   = (i == 0);
        bool isCard= (i % 2 == 0);
        p.setPen(isN ? Qt::red : Qt::white);
        p.setFont(QFont("Arial", isCard ? 9 : 7, isN ? QFont::Bold : QFont::Normal));
        p.drawText(QRectF(lx - 12, ly - 8, 24, 16), Qt::AlignCenter, labels[i]);
    }
    p.restore();

    // ── 상단 고정 빨간 삼각 마커 ──────────────────────────────
    QPolygonF tri;
    tri << QPointF(cx,     cy - r + 2)
        << QPointF(cx - 6, cy - r + 14)
        << QPointF(cx + 6, cy - r + 14);
    p.setBrush(Qt::red);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);

    // ── 헤딩 수치 ─────────────────────────────────────────────
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 13, QFont::Bold));
    QString hdg = QString("%1°").arg(static_cast<int>(_headingDeg) % 360);
    p.drawText(QRectF(cx - 30, cy - 12, 60, 24), Qt::AlignCenter, hdg);

    // ── 테두리 ────────────────────────────────────────────────
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(70, 70, 85), 2));
    p.drawEllipse(QPointF(cx, cy), r, r);
}
