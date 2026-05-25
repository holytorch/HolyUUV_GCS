import QtQuick 2.12

// ─────────────────────────────────────────────────────────────────────────────
// MissionStickPad
// 원형 조이스틱 패드. 마우스로 노브를 드래그하면 정규화된 (x, y)를
// stickChanged 시그널로 발신한다. 놓으면 자동으로 중앙 복귀(0,0).
//
// Operations 탭의 StickPad C++ 위젯과 동일한 동작:
//   x ∈ [-1, 1], y ∈ [-1, 1]
//   중심에서 ±5% (deadband) 안은 0으로 클램프
//   반경 밖으로 드래그하면 원의 둘레에 클램프
// ─────────────────────────────────────────────────────────────────────────────
Item {
    id: stickPad

    width: 110; height: 110

    // 현재 정규화 값 (외부 readonly, 갱신은 내부에서만)
    property real xNorm: 0
    property real yNorm: 0

    // 중심부 deadband (Operations 탭 JoystickWidget::DEADBAND = 0.05와 일치)
    property real deadband: 0.05

    property color bgColor:     "#B31a2530"
    property color borderColor: "#2a3540"
    property color knobColor:   "#2a3540"
    property color knobBorder:  "#4a5560"
    // 활성(드래그 중) 시 노브 강조 색
    property color knobActiveBorder: "#61d3ff"

    signal stickChanged(real x, real y)

    // 외부 라벨용
    property alias labelText: padLabel.text
    property alias labelVisible: padLabel.visible

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: stickPad.bgColor
        border.color: stickPad.borderColor
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8
    }

    // 가운데 십자선 (미세한 시각 가이드)
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.5
        height: 1
        color: "#2a3540"
        opacity: 0.4
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: 1
        height: parent.height * 0.5
        color: "#2a3540"
        opacity: 0.4
    }

    // 노브
    Rectangle {
        id: knob
        width: 32; height: 32; radius: 16
        color: stickPad.knobColor
        border.color: dragArea.pressed ? stickPad.knobActiveBorder : stickPad.knobBorder
        border.width: dragArea.pressed ? 2 : 1
        antialiasing: true
        // xNorm/yNorm을 중심 기준 픽셀 좌표로 환산
        x: stickPad.width / 2 - width / 2 + stickPad.xNorm * (stickPad.width / 2 - width / 2)
        y: stickPad.height / 2 - height / 2 + stickPad.yNorm * (stickPad.height / 2 - height / 2)

        // 부드러운 복귀
        Behavior on x { enabled: !dragArea.pressed; NumberAnimation { duration: 120 } }
        Behavior on y { enabled: !dragArea.pressed; NumberAnimation { duration: 120 } }
    }

    Text {
        id: padLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: 4
        text: ""
        color: "#cccccc"
        font.pixelSize: 10
        font.letterSpacing: 0.6
        visible: text.length > 0
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent

        // 드래그 가능 반경 — 노브 중심이 패드 안에 머무르도록
        property real radius: parent.width / 2 - knob.width / 2

        onPressed:         _update(mouse.x, mouse.y)
        onPositionChanged: if (pressed) _update(mouse.x, mouse.y)
        onReleased: {
            stickPad.xNorm = 0
            stickPad.yNorm = 0
            stickPad.stickChanged(0, 0)
        }

        function _update(mx, my) {
            var cx = stickPad.width  / 2
            var cy = stickPad.height / 2
            var dx = mx - cx
            var dy = my - cy
            var r  = Math.sqrt(dx * dx + dy * dy)
            // 반경 밖으로 나가면 원주에 클램프
            if (r > radius && radius > 0) {
                dx = dx * radius / r
                dy = dy * radius / r
            }
            var nx = (radius > 0) ? dx / radius : 0
            var ny = (radius > 0) ? dy / radius : 0
            // deadband
            if (Math.abs(nx) < stickPad.deadband) nx = 0
            if (Math.abs(ny) < stickPad.deadband) ny = 0
            stickPad.xNorm = nx
            stickPad.yNorm = ny
            stickPad.stickChanged(nx, ny)
        }
    }
}
