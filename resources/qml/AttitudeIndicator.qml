import QtQuick 2.12

// 미니멀 라인 기반 자세 지시계 (roll/pitch)
// 오퍼레이션 탭 AttitudeIndicator (QPainter)의 QML 미니멀 버전
Item {
    id: root
    property real rollDeg:  0
    property real pitchDeg: 0
    property color lineColor:    "#cccccc"
    property color accentColor:  "#61d3ff"
    property real  lineWidth:    1.2

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        property real rollVal:  root.rollDeg
        property real pitchVal: root.pitchDeg
        onRollValChanged:  requestPaint()
        onPitchValChanged: requestPaint()
        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var cx = width / 2
            var cy = height / 2
            var r  = Math.min(cx, cy) - 4

            // ── 1) 회전/피치 변환 + 클립 영역 안에 호라이즌 라인 + 피치 ladder ─
            ctx.save()
            // 원형 클리핑
            ctx.beginPath()
            ctx.arc(cx, cy, r - 2, 0, Math.PI * 2)
            ctx.clip()

            ctx.translate(cx, cy)
            ctx.rotate(-rollVal * Math.PI / 180)
            var pixPerDeg = (r * 2) / 45
            ctx.translate(0, pitchVal * pixPerDeg)

            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = root.lineWidth

            // 수평선
            ctx.beginPath()
            ctx.moveTo(-r * 2, 0)
            ctx.lineTo(r * 2, 0)
            ctx.stroke()

            // 피치 ladder
            ctx.font = "8px sans-serif"
            ctx.fillStyle = root.lineColor
            ctx.textBaseline = "middle"
            for (var deg = -40; deg <= 40; deg += 5) {
                if (deg === 0) continue
                var y = -deg * pixPerDeg
                var lineHalf = (deg % 10 === 0) ? r * 0.38 : r * 0.22
                ctx.beginPath()
                ctx.moveTo(-lineHalf, y)
                ctx.lineTo(lineHalf, y)
                ctx.stroke()
                if (deg % 10 === 0) {
                    var label = Math.abs(deg).toString()
                    ctx.textAlign = "left"
                    ctx.fillText(label, lineHalf + 3, y)
                    ctx.textAlign = "right"
                    ctx.fillText(label, -lineHalf - 3, y)
                }
            }
            ctx.restore()

            // ── 2) 롤 눈금 호 (외곽) ─────────────────────────────
            var rollTicks = [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60]
            for (var i = 0; i < rollTicks.length; i++) {
                var d = rollTicks[i]
                ctx.save()
                ctx.translate(cx, cy)
                ctx.rotate(d * Math.PI / 180)
                var tr = r * 0.92
                var len = (d % 30 === 0 || Math.abs(d) === 45) ? 7 : 4
                ctx.strokeStyle = root.lineColor
                ctx.lineWidth = root.lineWidth
                ctx.beginPath()
                ctx.moveTo(0, -tr)
                ctx.lineTo(0, -tr - len)
                ctx.stroke()
                ctx.restore()
            }

            // ── 3) 롤 포인터 (선 형태) ────────────────────────────
            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(-rollVal * Math.PI / 180)
            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = root.lineWidth + 0.3
            ctx.beginPath()
            ctx.moveTo(0, -r * 0.86)
            ctx.lineTo(-3.5, -r * 0.78)
            ctx.lineTo(3.5, -r * 0.78)
            ctx.closePath()
            ctx.stroke()
            ctx.restore()

            // ── 4) 고정 크로스헤어 (액센트 컬러 선) ──────────────
            ctx.strokeStyle = root.accentColor
            ctx.lineWidth = root.lineWidth + 0.4
            var arm = r * 0.32
            var gap = r * 0.08
            // 좌익
            ctx.beginPath()
            ctx.moveTo(cx - arm - gap, cy)
            ctx.lineTo(cx - gap, cy)
            ctx.stroke()
            // 우익
            ctx.beginPath()
            ctx.moveTo(cx + gap, cy)
            ctx.lineTo(cx + arm + gap, cy)
            ctx.stroke()
            // 중앙 점
            ctx.fillStyle = root.accentColor
            ctx.beginPath()
            ctx.arc(cx, cy, 1.5, 0, Math.PI * 2)
            ctx.fill()

            // ── 5) 외곽 원 (테두리) ─────────────────────────────
            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = root.lineWidth
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.stroke()
        }
    }
}
