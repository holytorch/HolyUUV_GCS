#include "DepthGauge.h"

#include <QPainter>
#include <QLinearGradient>

DepthGauge::DepthGauge(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(70, 200);
}

void DepthGauge::setDepth(float depthM)
{
    _depth = depthM;
    update();
}

void DepthGauge::setMaxDepth(float maxDepthM)
{
    _maxDepth = maxDepthM;
    update();
}

void DepthGauge::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 레이아웃 상수
    const int labelW = 26;
    const int barX   = labelW;
    const int barW   = width() - labelW - 6;
    const int titleH = 22;
    const int valueH = 22;
    const int barY   = titleH;
    const int barH   = height() - titleH - valueH;

    // ── 배경 ──────────────────────────────────────────────────
    p.fillRect(rect(), QColor(18, 20, 32));

    // ── 바 배경 ───────────────────────────────────────────────
    p.setBrush(QColor(38, 40, 55));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(barX, barY, barW, barH, 4, 4);

    // ── 채움 바 (파란 그라디언트) ─────────────────────────────
    float ratio = (_maxDepth > 0) ? qBound(0.0f, _depth / _maxDepth, 1.0f) : 0.0f;
    int   fillH = static_cast<int>(barH * ratio);
    if (fillH > 0) {
        QLinearGradient grad(0, barY, 0, barY + barH);
        grad.setColorAt(0.0, QColor(0, 190, 255));
        grad.setColorAt(1.0, QColor(0,  50, 160));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barX, barY, barW, fillH, 4, 4);
    }

    // ── 눈금 및 레이블 ────────────────────────────────────────
    p.setFont(QFont("Arial", 7));
    constexpr int steps = 5;
    for (int i = 0; i <= steps; i++) {
        int   y     = barY + barH * i / steps;
        float depth = _maxDepth * i / steps;
        p.setPen(QPen(QColor(100, 100, 120), 1));
        p.drawLine(barX - 4, y, barX, y);
        p.setPen(Qt::white);
        p.drawText(QRect(0, y - 6, labelW - 5, 12),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(static_cast<int>(depth)));
    }

    // ── 제목 ──────────────────────────────────────────────────
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.drawText(QRect(0, 3, width(), titleH - 3), Qt::AlignCenter, "DEPTH");

    // ── 현재 수심 수치 ────────────────────────────────────────
    p.setFont(QFont("Arial", 10, QFont::Bold));
    p.setPen(QColor(0, 220, 255));
    // 수심은 음수로 표시 (예: -5.3 m). 바 시각화는 양수 magnitude 그대로 사용.
    p.drawText(QRect(0, height() - valueH, width(), valueH),
               Qt::AlignCenter,
               QString("%1m").arg(-_depth, 0, 'f', 1));
}
