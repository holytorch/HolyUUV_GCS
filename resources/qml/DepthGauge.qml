import QtQuick 2.12

// 미니멀 라인 기반 수심 게이지 (line-only)
Item {
    id: root
    property real depth:    0.0     // 현재 수심 (m, 양수)
    property real maxDepth: 100.0   // 최대 표시 수심
    property color lineColor:   "#cccccc"
    property color accentColor: "#61d3ff"
    property real  lineWidth:   1.2

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        property real depthVal: root.depth
        onDepthValChanged: requestPaint()
        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var titleH = 22
            var valueH = 26
            var leftPad = 30
            var rightPad = 18
            var barW = (width - leftPad - rightPad) / 2 + 10
            var barX = width - barW - rightPad
            var barY = titleH
            var barH = height - titleH - valueH

            // ── 타이틀 "DEPTH" (막대 중앙 위) ───────────────────
            ctx.fillStyle = root.lineColor
            ctx.font = "bold 12px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "top"
            ctx.fillText("DEPTH", barX + barW / 2, 2)

            // ── 외곽 바 (세로 직사각형 선) ──────────────────────
            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = root.lineWidth
            ctx.beginPath()
            ctx.rect(barX, barY, barW, barH)
            ctx.stroke()

            // ── 눈금 + 라벨 (5단계) ─────────────────────────────
            var steps = 5
            ctx.font = "8px sans-serif"
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"
            for (var i = 0; i <= steps; i++) {
                var y = barY + barH * i / steps
                var depthAtTick = root.maxDepth * i / steps

                // 눈금선 (바 왼쪽 바깥쪽으로)
                ctx.beginPath()
                ctx.moveTo(barX - 4, y)
                ctx.lineTo(barX, y)
                ctx.stroke()

                // 라벨
                ctx.fillStyle = root.lineColor
                ctx.fillText(Math.round(depthAtTick).toString(), barX - 6, y)
            }

            // ── 현재 수심 인디케이터 ─────────────────────────────
            var ratio = Math.max(0, Math.min(1, root.depth / root.maxDepth))
            var indicatorY = barY + barH * ratio

            ctx.strokeStyle = root.accentColor
            ctx.lineWidth = root.lineWidth + 0.5

            // 좌측 화살표(▶)
            ctx.beginPath()
            ctx.moveTo(barX + 1, indicatorY - 4)
            ctx.lineTo(barX + 6, indicatorY)
            ctx.lineTo(barX + 1, indicatorY + 4)
            ctx.stroke()

            // 바 안에 가로 라인
            ctx.beginPath()
            ctx.moveTo(barX + 7, indicatorY)
            ctx.lineTo(barX + barW - 2, indicatorY)
            ctx.stroke()

            // ── 현재 수심 수치 ──────────────────────────────────
            ctx.fillStyle = root.accentColor
            ctx.font = "bold 14px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "bottom"
            ctx.fillText(root.depth.toFixed(1) + "m", barX + barW / 2, height - 4)
        }
    }
}
