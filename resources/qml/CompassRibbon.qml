import QtQuick 2.12

// 수평 나침반 리본 (Tape Compass)
// heading 프로퍼티 (0~360°)에 따라 눈금이 좌우로 흐른다.
Rectangle {
    id: ribbon
    property real heading: 15.4

    // 픽셀당 회전각 (작을수록 빽빽함)
    property real pxPerDeg: 6.0

    width: 320
    height: 62
    radius: 14
    color: "#B31a2530"
    border.color: "#2a3540"
    border.width: 1

    // 상단 헤딩 표시 (N 15.4°)
    Row {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 6
        spacing: 10

        Text {
            text: {
                var h = ((ribbon.heading % 360) + 360) % 360
                if (h < 22.5  || h >= 337.5) return "N"
                if (h < 67.5)  return "NE"
                if (h < 112.5) return "E"
                if (h < 157.5) return "SE"
                if (h < 202.5) return "S"
                if (h < 247.5) return "SW"
                if (h < 292.5) return "W"
                return "NW"
            }
            color: "#eef4f5"
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
        Text {
            text: ribbon.heading.toFixed(1) + "°"
            color: "#dfe9eb"
            font.pixelSize: 12
            font.family: "monospace"
        }
    }

    // 중앙 인디케이터 ▼
    Canvas {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 24
        width: 10; height: 6
        Component.onCompleted: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#ffffff"
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width/2, height)
            ctx.closePath()
            ctx.fill()
        }
    }

    // 가장자리 카디널 라벨 (현재 heading 사분면 양쪽 끝 표시)
    Text {
        id: leftCardinal
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -8
        color: "#eef4f5"
        font.pixelSize: 13
        font.weight: Font.Bold
        text: {
            var h = ((ribbon.heading % 360) + 360) % 360
            if (h < 90)  return "N"
            if (h < 180) return "E"
            if (h < 270) return "S"
            return "W"
        }
    }
    Text {
        id: rightCardinal
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -8
        color: "#eef4f5"
        font.pixelSize: 13
        font.weight: Font.Bold
        text: {
            var h = ((ribbon.heading % 360) + 360) % 360
            if (h < 90)  return "E"
            if (h < 180) return "S"
            if (h < 270) return "W"
            return "N"
        }
    }

    // 눈금 영역 (좌우 클리핑)
    Item {
        id: tickArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 6
        height: 28
        clip: true

        Canvas {
            id: tickCanvas
            anchors.fill: parent
            // heading 변경 시 다시 그림
            property real headingValue: ribbon.heading

            onHeadingValueChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                var cx       = width / 2
                var pxPerDeg = ribbon.pxPerDeg
                var halfW    = width / 2
                var halfDeg  = halfW / pxPerDeg
                var arcDepth = 8

                // 눈금 시작 y (위쪽에 N/E/S/W 라벨 공간 확보)
                var tickTop  = 12

                ctx.strokeStyle = "#dfe9eb"
                ctx.fillStyle   = "#dfe9eb"
                ctx.lineWidth   = 1
                ctx.textAlign   = "center"
                ctx.font        = "bold 11px sans-serif"

                for (var d = Math.floor((headingValue - halfDeg) / 5) * 5;
                     d <= headingValue + halfDeg;
                     d += 5)
                {
                    var x       = cx + (d - headingValue) * pxPerDeg
                    var norm    = ((d % 360) + 360) % 360
                    var isMajor = (norm % 30 === 0)
                    var len     = isMajor ? 14 : 6

                    // 호 곡률
                    var tNorm = (x - cx) / halfW
                    var yOff  = arcDepth * tNorm * tNorm

                    // 카디널 라벨 (0/90/180/270)
                    var label = norm === 0   ? "N"
                              : norm === 90  ? "E"
                              : norm === 180 ? "S"
                              : norm === 270 ? "W"
                              : null
                    if (label) {
                        ctx.fillText(label, x, tickTop + yOff - 2)
                    }

                    ctx.beginPath()
                    ctx.moveTo(x, tickTop + yOff)
                    ctx.lineTo(x, tickTop + yOff + len)
                    ctx.stroke()
                }
            }
        }
    }
}
