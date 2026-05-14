#include "AttitudeIndicator.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <cmath>

AttitudeIndicator::AttitudeIndicator(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
}

void AttitudeIndicator::setAttitude(float rollDeg, float pitchDeg)
{
    _rollDeg  = rollDeg;
    _pitchDeg = pitchDeg;
    update();
}

void AttitudeIndicator::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal cx = width()  / 2.0;
    const qreal cy = height() / 2.0;
    const qreal r  = qMin(cx, cy) - 3;

    // ── 원형 클립 ─────────────────────────────────────────────
    QPainterPath clip;
    clip.addEllipse(QPointF(cx, cy), r, r);
    p.setClipPath(clip);

    // ── 롤/피치 변환 + 하늘·지면 블록 그리기 ──────────────────
    // QGC와 동일: 블록은 위젯보다 4배 넓고 8배 높아서 회전해도 빈 틈이 없음
    p.save();
    p.translate(cx, cy);
    p.rotate(-_rollDeg);
    // 피치 1°당 이동 픽셀: QGC 공식 height / 45
    qreal pitchOffset = _pitchDeg * (r * 2.0) / 45.0;
    p.translate(0, pitchOffset);

    qreal bw = r * 4, bh = r * 8;

    QLinearGradient skyGrad(0, -bh, 0, 0);
    skyGrad.setColorAt(0.25, QColor::fromHslF(0.60, 1.0, 0.25));
    skyGrad.setColorAt(0.50, QColor::fromHslF(0.60, 0.5, 0.55));
    p.fillRect(QRectF(-bw / 2, -bh, bw, bh), skyGrad);

    QLinearGradient groundGrad(0, 0, 0, bh / 2);
    groundGrad.setColorAt(0.00, QColor::fromHslF(0.25, 0.50, 0.45));
    groundGrad.setColorAt(0.25, QColor::fromHslF(0.25, 0.75, 0.25));
    p.fillRect(QRectF(-bw / 2, 0, bw, bh / 2), groundGrad);

    // 수평선
    p.setPen(QPen(Qt::white, 2));
    p.drawLine(QPointF(-bw / 2, 0), QPointF(bw / 2, 0));

    // 피치 래더 (5° 간격, 레이블 10° 간격)
    qreal pixPerDeg = (r * 2.0) / 45.0;
    p.setFont(QFont("Arial", 8));
    for (int deg = -40; deg <= 40; deg += 5) {
        if (deg == 0) continue;
        qreal y       = -deg * pixPerDeg;
        qreal lineHalf = (deg % 10 == 0) ? r * 0.38 : r * 0.22;
        p.setPen(QPen(Qt::white, 1));
        p.drawLine(QPointF(-lineHalf, y), QPointF(lineHalf, y));
        if (deg % 10 == 0) {
            QString txt = QString::number(qAbs(deg));
            p.drawText(QRectF(lineHalf + 3, y - 6, 18, 12),
                       Qt::AlignLeft | Qt::AlignVCenter, txt);
            p.drawText(QRectF(-lineHalf - 21, y - 6, 18, 12),
                       Qt::AlignRight | Qt::AlignVCenter, txt);
        }
    }
    p.restore();

    // ── 롤 눈금 호 (클립 해제 후 그림) ───────────────────────
    p.setClipping(false);

    // 눈금: ±10, ±20, ±30, ±45, ±60°
    for (int deg : {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60}) {
        p.save();
        p.translate(cx, cy);
        p.rotate(deg);
        qreal tr      = r * 0.91;
        qreal tickLen = (deg % 30 == 0 || deg == -45 || deg == 45) ? 10 : 6;
        p.setPen(QPen(Qt::white, 1));
        p.drawLine(QPointF(0, -tr), QPointF(0, -tr - tickLen));
        p.restore();
    }

    // 롤 포인터 삼각형 (롤에 따라 회전)
    p.save();
    p.translate(cx, cy);
    p.rotate(-_rollDeg);
    qreal tr = r * 0.88;
    QPolygonF tri;
    tri << QPointF(0, -tr - 10) << QPointF(-5, -tr) << QPointF(5, -tr);
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
    p.restore();

    // ── 고정 크로스헤어 (노란색 날개) ─────────────────────────
    p.setPen(QPen(Qt::yellow, 2));
    qreal arm = r * 0.35, gap = r * 0.10;
    // 왼쪽 날개
    p.drawLine(QPointF(cx - arm - gap, cy), QPointF(cx - gap, cy));
    // 오른쪽 날개
    p.drawLine(QPointF(cx + gap, cy), QPointF(cx + arm + gap, cy));
    // 중앙 짧은 세로선
    p.drawLine(QPointF(cx, cy - gap), QPointF(cx, cy + gap));
    // 중앙 점
    p.setBrush(Qt::yellow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), 3, 3);

    // ── 원형 테두리 ───────────────────────────────────────────
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(80, 80, 90), 3));
    p.drawEllipse(QPointF(cx, cy), r, r);
}
