import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12
import QtLocation 5.12
import QtPositioning 5.12

Rectangle {
    anchors.fill: parent
    color: "#0d1620"

    // false = OSM dark, true = Voyager light
    property bool useLightMap: false

    // 두 맵이 공유하는 center/zoom (토글 시 위치/줌 유지)
    property var mapCenter: QtPositioning.coordinate(35.074857, 129.084836)
    property real mapZoom: 15

    // ── 배경: 한 번에 한 맵만 로드 (메모리 절약) ─────────────────
    Loader {
        id: mapLoader
        anchors.fill: parent
        sourceComponent: useLightMap ? voyagerComponent : osmComponent
    }

    Component {
        id: osmComponent
        Map {
            plugin: Plugin {
                name: "osm"
                PluginParameter {
                    name: "osm.mapping.custom.host"
                    value: "http://127.0.0.1:17777/osm/"
                }
                PluginParameter {
                    name: "osm.mapping.cache.directory"
                    value: bridge ? bridge.tileCachePath + "/osm" : "/tmp/holyuuv_tiles/osm"
                }
                PluginParameter { name: "osm.mapping.cache.disk.cost_strategy"; value: "unitary" }
            }

            center: mapCenter
            zoomLevel: mapZoom

            onCenterChanged:    mapCenter = center
            onZoomLevelChanged: mapZoom   = zoomLevel

            Component.onCompleted: {
                for (var i = 0; i < supportedMapTypes.length; i++) {
                    if (supportedMapTypes[i].style === MapType.CustomMap) {
                        activeMapType = supportedMapTypes[i]
                        break
                    }
                }
            }

            MapQuickItem {
                visible: bridge ? bridge.hasPosition : false
                coordinate: QtPositioning.coordinate(
                    bridge ? bridge.latitude  : 0,
                    bridge ? bridge.longitude : 0)
                anchorPoint.x: 8
                anchorPoint.y: 8
                sourceItem: Rectangle {
                    width: 16; height: 16; radius: 8
                    color: "#00e5ff"
                    border.color: "white"
                    border.width: 2
                }
            }
        }
    }

    Component {
        id: voyagerComponent
        Map {
            plugin: Plugin {
                name: "osm"
                PluginParameter {
                    name: "osm.mapping.custom.host"
                    value: "http://127.0.0.1:17777/voyager/"
                }
                PluginParameter {
                    name: "osm.mapping.cache.directory"
                    value: bridge ? bridge.tileCachePath + "/voyager" : "/tmp/holyuuv_tiles/voyager"
                }
                PluginParameter { name: "osm.mapping.cache.disk.cost_strategy"; value: "unitary" }
            }

            center: mapCenter
            zoomLevel: mapZoom

            onCenterChanged:    mapCenter = center
            onZoomLevelChanged: mapZoom   = zoomLevel

            Component.onCompleted: {
                for (var i = 0; i < supportedMapTypes.length; i++) {
                    if (supportedMapTypes[i].style === MapType.CustomMap) {
                        activeMapType = supportedMapTypes[i]
                        break
                    }
                }
            }

            MapQuickItem {
                visible: bridge ? bridge.hasPosition : false
                coordinate: QtPositioning.coordinate(
                    bridge ? bridge.latitude  : 0,
                    bridge ? bridge.longitude : 0)
                anchorPoint.x: 8
                anchorPoint.y: 8
                sourceItem: Rectangle {
                    width: 16; height: 16; radius: 8
                    color: "#00e5ff"
                    border.color: "white"
                    border.width: 2
                }
            }
        }
    }

    // 좌상단: 로고
    Item {
        id: logoImage
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 16
        anchors.leftMargin: 16
        width: 44; height: 44

        Image {
            id: logoSrc
            anchors.fill: parent
            source: "qrc:/assets/HolyUUV_GCS.png"
            sourceSize.width: 128
            sourceSize.height: 128
            smooth: true
            antialiasing: true
            mipmap: true
            fillMode: Image.PreserveAspectCrop
            visible: false
        }

        Rectangle {
            id: logoMask
            anchors.fill: parent
            radius: 12
            visible: false
        }

        OpacityMask {
            anchors.fill: parent
            source: logoSrc
            maskSource: logoMask
        }
    }

    // 차량 목록 (목데이터 — 나중에 C++ 모델로 교체)
    ListModel {
        id: vehiclesModel
        // 초기에는 비어있음. Add Vehicle로 추가.
    }
    property int activeVehicleIndex: -1

    // 로고 우측: SysID 카드 (클릭 시 차량 목록 팝업 토글)
    Rectangle {
        id: sysIdCard
        anchors.top: parent.top
        anchors.left: logoImage.right
        anchors.topMargin: 16
        anchors.leftMargin: 12
        width: sysIdRow.implicitWidth + 28
        height: 44
        radius: 12
        color: sysIdMa.containsMouse ? "#B3243341" : "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8

        Row {
            id: sysIdRow
            anchors.centerIn: parent
            spacing: 20

            Text {
                text: activeVehicleIndex >= 0
                    ? "sys_id: " + vehiclesModel.get(activeVehicleIndex).sysid
                    : "sys_id : -"
                color: "#cccccc"
                font.pixelSize: 12
                font.letterSpacing: 0.6
                font.family: "sans-serif"
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "▾"
                color: "#cccccc"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            id: sysIdMa
            anchors.fill: parent
            hoverEnabled: true
            onClicked: sysIdPopup.opened ? sysIdPopup.close() : sysIdPopup.open()
        }

        // 차량 목록 팝업
        Popup {
            id: sysIdPopup
            x: 0
            y: sysIdCard.height + 6
            width: 260
            padding: 0
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape

            background: Rectangle {
                color: "#B31a2530"
                border.color: "#2a3540"
                border.width: 1
                radius: 12
            }

            contentItem: Item {
                // 모서리 둥글게 클리핑 (Add Vehicle 호버 시 직사각 모서리 방지)
                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Rectangle {
                        width: sysIdPopup.width
                        height: sysIdPopup.height
                        radius: 12
                    }
                }

                Column {
                    anchors.fill: parent
                    spacing: 0

                Text {
                    padding: 14
                    text: "VEHICLES"
                    color: "#8faabc"
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                }

                // 차량 리스트
                Repeater {
                    model: vehiclesModel
                    Rectangle {
                        width: sysIdPopup.width
                        height: 36
                        color: ma.containsMouse ? "#B3243341" : "transparent"

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 8
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: index === activeVehicleIndex ? "✓" : "  "
                                color: "#44bb44"
                                font.pixelSize: 12
                                width: 12
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "sysid: " + sysid + (name ? "  (" + name + ")" : "")
                                color: "#cccccc"
                                font.pixelSize: 12
                                font.family: "monospace"
                            }
                        }

                        MouseArea {
                            id: ma
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                activeVehicleIndex = index
                                sysIdPopup.close()
                            }
                        }
                    }
                }

                Rectangle {
                    width: sysIdPopup.width
                    height: 1
                    color: "#2a3540"
                }

                // + Add Vehicle
                Rectangle {
                    width: sysIdPopup.width
                    height: 40
                    color: addMa.containsMouse ? "#B3243341" : "transparent"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        verticalAlignment: Text.AlignVCenter
                        text: "＋  Add Vehicle"
                        color: "#61d3ff"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: addMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            sysIdPopup.close()
                            addVehicleDialog.open()
                        }
                    }
                }
                }   // Column 닫기
            }       // 래핑 Item 닫기
        }
    }

    // ── Add Vehicle 모달 다이얼로그 ─────────────────────────────────
    Popup {
        id: addVehicleDialog
        anchors.centerIn: parent
        width: 320
        padding: 20
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape

        background: Rectangle {
            color: "#B31a2530"
            border.color: "#2a3540"
            border.width: 1
            radius: 14
        }

        property string connType: "UDP"

        contentItem: Column {
            spacing: 14

            Text {
                text: "Add Vehicle"
                color: "#dfe9eb"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            // Connection type
            Row {
                spacing: 18
                Text { text: "Connection:"; color: "#8faabc"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                Repeater {
                    model: ["UDP", "TCP"]
                    Row {
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 12; height: 12; radius: 6
                            border.color: "#61d3ff"; border.width: 1
                            color: "transparent"
                            anchors.verticalCenter: parent.verticalCenter
                            Rectangle {
                                anchors.centerIn: parent
                                width: 6; height: 6; radius: 3
                                color: "#61d3ff"
                                visible: modelData === addVehicleDialog.connType
                            }
                            MouseArea { anchors.fill: parent; onClicked: addVehicleDialog.connType = modelData }
                        }
                        Text {
                            text: modelData; color: "#cccccc"; font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // Host
            Column {
                spacing: 4
                width: parent.width
                Text { text: "Host"; color: "#8faabc"; font.pixelSize: 11 }
                TextField {
                    id: hostField
                    width: parent.width
                    text: "127.0.0.1"
                    color: "#dfe9eb"
                    font.pixelSize: 12
                    font.family: "monospace"
                    background: Rectangle {
                        color: "#0d1620"
                        border.color: "#2a3540"
                        border.width: 1
                        radius: 6
                    }
                }
            }

            // Port
            Column {
                spacing: 4
                width: parent.width
                Text { text: "Port"; color: "#8faabc"; font.pixelSize: 11 }
                TextField {
                    id: portField
                    width: parent.width
                    text: "14550"
                    color: "#dfe9eb"
                    font.pixelSize: 12
                    font.family: "monospace"
                    background: Rectangle {
                        color: "#0d1620"
                        border.color: "#2a3540"
                        border.width: 1
                        radius: 6
                    }
                }
            }

            // 버튼
            Row {
                spacing: 8
                anchors.right: parent.right

                Rectangle {
                    width: 80; height: 30; radius: 8
                    color: cancelMa.containsMouse ? "#B3243341" : "transparent"
                    border.color: "#2a3540"; border.width: 1
                    Text { anchors.centerIn: parent; text: "Cancel"; color: "#cccccc"; font.pixelSize: 12 }
                    MouseArea {
                        id: cancelMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: addVehicleDialog.close()
                    }
                }

                Rectangle {
                    width: 90; height: 30; radius: 8
                    color: connectMa.containsMouse ? "#3eaedb" : "#61d3ff"
                    Text { anchors.centerIn: parent; text: "Connect"; color: "#0d1620"; font.pixelSize: 12; font.weight: Font.DemiBold }
                    MouseArea {
                        id: connectMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            // 목데이터: 목록에 추가 (실제 연결은 추후 C++ 연동)
                            var nextSysid = vehiclesModel.count + 1
                            vehiclesModel.append({
                                sysid: nextSysid,
                                name: "",
                                host: hostField.text,
                                port: portField.text,
                                type: addVehicleDialog.connType
                            })
                            activeVehicleIndex = vehiclesModel.count - 1
                            addVehicleDialog.close()
                        }
                    }
                }
            }
        }
    }

    // SysID 우측: 링크 상태 카드 (텍스트 길이에 따라 가변폭)
    Rectangle {
        id: linkCard
        property bool linkOk: false   // 추후 VehicleState::heartbeatOk 바인딩

        anchors.top: parent.top
        anchors.left: sysIdCard.right
        anchors.topMargin: 16
        anchors.leftMargin: 12
        width: linkRow.implicitWidth + 28
        height: 44
        radius: 12
        color: "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8

        Row {
            id: linkRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: "●"
                color: linkCard.linkOk ? "#44bb44" : "#ff4444"
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: linkCard.linkOk ? "LINKED" : "NO LINKED"
                color: linkCard.linkOk ? "#cccccc" : "#ff4444"
                font.pixelSize: 12
                font.weight: Font.Normal
                font.letterSpacing: 0.6
                font.family: "sans-serif"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Link 우측: ARM 상태 카드
    Rectangle {
        id: armCard
        property bool armed: false   // 추후 VehicleState::armed 바인딩

        anchors.top: parent.top
        anchors.left: linkCard.right
        anchors.topMargin: 16
        anchors.leftMargin: 12
        width: armRow.implicitWidth + 28
        height: 44
        radius: 12
        color: "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8

        Row {
            id: armRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: "●"
                color: "#ff4444"
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: armCard.armed ? "ARMED" : "DISARMED"
                color: "#ff4444"
                font.pixelSize: 12
                font.weight: armCard.armed ? Font.DemiBold : Font.Normal
                font.letterSpacing: 0.6
                font.family: "sans-serif"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // 우상단: 위경도 카드 (3아이콘 버튼 좌측에 배치)
    Rectangle {
        id: latLonCard
        anchors.top: parent.top
        anchors.right: iconRow.left
        anchors.topMargin: 16
        anchors.rightMargin: 12
        width: 250
        height: 44
        radius: 12
        color: "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8

        Text {
            anchors.centerIn: parent
            text: "46.4883333° N,  30.7397123° E"
            color: "#cccccc"
            font.pixelSize: 12
            font.weight: Font.Normal
            font.letterSpacing: 0.4
            font.family: "monospace"
        }
    }

    // 상단 중앙: 나침반 리본
    CompassRibbon {
        id: compass
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 16
        heading: 15.4
    }

    // GPS 카드 밑: 다크/라이트 맵 토글 버튼
    Rectangle {
        id: mapToggleBtn
        anchors.top: iconRow.bottom
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.rightMargin: 16
        width: 44; height: 44
        radius: 12
        color: mapToggleMa.containsMouse ? "#B3243341" : "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8

        // 카드 안에 살짝 여백 두고 이미지 배치
        Item {
            anchors.fill: parent
            anchors.margins: 6

            Image {
                id: mapPreviewSrc
                anchors.fill: parent
                source: "qrc:/assets/map.png"
                sourceSize.width: 128
                sourceSize.height: 128
                smooth: true
                antialiasing: true
                mipmap: true
                fillMode: Image.PreserveAspectCrop
                visible: false
            }
            Rectangle {
                id: mapPreviewMask
                anchors.fill: parent
                radius: 8
                visible: false
            }
            OpacityMask {
                anchors.fill: parent
                source: mapPreviewSrc
                maskSource: mapPreviewMask
            }
        }

        MouseArea {
            id: mapToggleMa
            anchors.fill: parent
            hoverEnabled: true
            onClicked: useLightMap = !useLightMap
        }
    }

    // ── 줌 컨트롤 (지도 토글 카드 밑) ──────────────────────────────
    // +  : zoom +1
    // 표시 : 현재 줌 (0.1 단위 사용 가능)
    // -  : zoom -1
    // 입력 : 직접 값 입력 (0.1 단위)
    Column {
        id: zoomControl
        anchors.top: mapToggleBtn.bottom
        anchors.right: parent.right
        anchors.topMargin: 40
        anchors.rightMargin: 16
        spacing: 4

        // + 버튼
        Rectangle {
            width: 44; height: 36
            radius: 12
            color: zoomPlusMa.containsMouse ? "#B3243341" : "#B31a2530"
            border.color: "#2a3540"
            border.width: 1
            antialiasing: true
            layer.enabled: true
            layer.samples: 8

            Text {
                anchors.centerIn: parent
                text: "+"
                color: "#cccccc"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: zoomPlusMa
                anchors.fill: parent
                hoverEnabled: true
                onClicked: mapZoom = Math.min(Math.round(mapZoom) + 1, 20)
            }
        }

        // 현재 줌 표시
        Rectangle {
            width: 44; height: 28
            radius: 10
            color: "#B31a2530"
            border.color: "#2a3540"
            border.width: 1
            antialiasing: true
            layer.enabled: true
            layer.samples: 8

            Text {
                anchors.centerIn: parent
                text: mapZoom.toFixed(1)
                color: "#cccccc"
                font.pixelSize: 11
                font.family: "monospace"
            }
        }

        // − 버튼
        Rectangle {
            width: 44; height: 36
            radius: 12
            color: zoomMinusMa.containsMouse ? "#B3243341" : "#B31a2530"
            border.color: "#2a3540"
            border.width: 1
            antialiasing: true
            layer.enabled: true
            layer.samples: 8

            Text {
                anchors.centerIn: parent
                text: "−"
                color: "#cccccc"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: zoomMinusMa
                anchors.fill: parent
                hoverEnabled: true
                onClicked: mapZoom = Math.max(Math.round(mapZoom) - 1, 0)
            }
        }

    }

    // 상단 우측 3개 아이콘 버튼
    RowLayout {
        id: iconRow
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 16
        anchors.rightMargin: 16
        spacing: 8

        // 위성 아이콘 (클릭 시 GPS 상태 팝업)
        Rectangle {
            id: gpsBtn
            width: 44; height: 44
            radius: 12
            color: gpsMa.containsMouse ? "#B3243341" : "#B31a2530"
            border.color: "#2a3540"
            border.width: 1

            Image {
                anchors.fill: parent
                anchors.margins: 4
                source: "qrc:/assets/satellite.png"
                sourceSize.width: 96
                sourceSize.height: 96
                smooth: true
                antialiasing: true
                mipmap: true
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: gpsMa
                anchors.fill: parent
                hoverEnabled: true
                onClicked: gpsPopup.opened ? gpsPopup.close() : gpsPopup.open()
            }

            // GPS 상세 정보 팝업
            Popup {
                id: gpsPopup
                x: gpsBtn.width - width
                y: gpsBtn.height + 6
                width: 240
                padding: 0
                modal: false
                focus: true
                closePolicy: Popup.CloseOnEscape

                background: Rectangle {
                    color: "#B31a2530"
                    border.color: "#2a3540"
                    border.width: 1
                    radius: 12
                }

                contentItem: Column {
                    spacing: 6
                    padding: 14

                    Text { text: "GPS STATUS"; color: "#8faabc"; font.pixelSize: 10; font.letterSpacing: 1.2 }
                    Text { text: "Fix:         3D";   color: "#cccccc"; font.pixelSize: 12; font.family: "monospace" }
                    Text { text: "Satellites:  12";   color: "#cccccc"; font.pixelSize: 12; font.family: "monospace" }
                    Text { text: "HDOP:        0.8";  color: "#cccccc"; font.pixelSize: 12; font.family: "monospace" }
                    Text { text: "VDOP:        1.2";  color: "#cccccc"; font.pixelSize: 12; font.family: "monospace" }
                }
            }
        }

    }

    // 로그 펼침/접힘 상태
    property bool logVisible: true

    // 조이스틱 표시 여부
    property bool joystickVisible: true

    // 로그 토글 버튼 (로그 보이면 카드 위, 숨기면 화면 하단으로)
    Rectangle {
        id: logToggleBtn
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 16
        // logVisible: 로그카드(160) + 카드↔버튼 갭(6) + bottomBar 하단 마진(16)
        // !logVisible: 화면 맨 아래 (마진 16)
        anchors.bottomMargin: logVisible ? (160 + 6 + 16) : 16
        width: 56; height: 24
        radius: 8
        color: logToggleMa.containsMouse ? "#B3243341" : "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8
        z: 10

        Row {
            anchors.centerIn: parent
            spacing: 4
            Text {
                text: logVisible ? "▼" : "▲"
                color: "#cccccc"
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "LOG"
                color: "#cccccc"
                font.pixelSize: 10
                font.letterSpacing: 1.0
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            id: logToggleMa
            anchors.fill: parent
            hoverEnabled: true
            onClicked: logVisible = !logVisible
        }
    }

    // ── 지도 위 떠 있는 조이스틱 (좌: Strafe/Fwd, 우: Yaw/Heave) ────
    Rectangle {
        id: leftJoystick
        visible: joystickVisible
        anchors.bottom: bottomBar.top
        anchors.left: bottomBar.left
        anchors.bottomMargin: 12
        anchors.leftMargin: 60
        width: 110; height: 110
        radius: 55
        color: "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8

        Rectangle {
            anchors.centerIn: parent
            width: 32; height: 32; radius: 16
            color: "#2a3540"
            border.color: "#4a5560"
            border.width: 1
            antialiasing: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 4
            text: "Strafe / Fwd"
            color: "#cccccc"
            font.pixelSize: 10
            font.letterSpacing: 0.6
        }
    }

    AttitudeIndicator {
        id: attitudeGauge
        anchors.bottom: bottomBar.top
        anchors.right: parent.right
        anchors.bottomMargin: 12
        anchors.rightMargin: 20
        width: 170; height: 170
        rollDeg: 0
        pitchDeg: 0
    }

    // 수심 게이지 (롤/피치 게이지 위, 동일 우측 마진)
    DepthGauge {
        id: depthGauge
        anchors.bottom: attitudeGauge.top
        anchors.right: parent.right
        anchors.bottomMargin: 12
        anchors.rightMargin: 12
        width: 100; height: 160
        depth: 5.4
        maxDepth: 100
    }

    Rectangle {
        id: rightJoystick
        visible: joystickVisible
        anchors.bottom: bottomBar.top
        anchors.right: attitudeGauge.left
        anchors.rightMargin: 50
        anchors.bottomMargin: 12
        width: 110; height: 110
        radius: 55
        color: "#B31a2530"
        border.color: "#2a3540"
        border.width: 1
        antialiasing: true
        layer.enabled: true
        layer.samples: 8

        Rectangle {
            anchors.centerIn: parent
            width: 32; height: 32; radius: 16
            color: "#2a3540"
            border.color: "#4a5560"
            border.width: 1
            antialiasing: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 4
            text: "Yaw / Heave"
            color: "#cccccc"
            font.pixelSize: 10
            font.letterSpacing: 0.6
        }
    }

    // ── 하단: 로그 + 텔레메트리 ─────────────────────────────────
    Item {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 16
        height: 160


        // 좌측: LOG 영역 (가로 절반)
        Rectangle {
            id: logCard
            visible: logVisible
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: parent.width / 2
            radius: 12
            color: "#B31a2530"
            border.color: "#2a3540"
            border.width: 1
            antialiasing: true
            layer.enabled: true
            layer.samples: 8

            Text {
                id: logTitle
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 10
                anchors.leftMargin: 14
                text: "LOG"
                color: "#8faabc"
                font.pixelSize: 10
                font.letterSpacing: 1.2
            }

            Flickable {
                id: logFlick
                anchors.top: logTitle.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.topMargin: 6
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.bottomMargin: 10
                clip: true

                contentWidth: width
                contentHeight: logText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                flickDeceleration: 4000
                maximumFlickVelocity: 2500

                // 마우스 휠 부드럽게
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    onWheel: {
                        var delta = wheel.angleDelta.y
                        var step = 40   // 한 틱당 픽셀
                        var target = logFlick.contentY - (delta / 120) * step
                        target = Math.max(0, Math.min(target, logFlick.contentHeight - logFlick.height))
                        logFlick.contentY = target
                    }
                }

                Text {
                    id: logText
                    width: logFlick.width
                    color: "#cccccc"
                    font.pixelSize: 11
                    font.family: "monospace"
                    wrapMode: Text.Wrap
                    text:
                        "[12:34:01] HEARTBEAT received sysid=1\n" +
                        "[12:34:01] GPS Fix: 3D, sats=12\n" +
                        "[12:34:02] ARMED\n" +
                        "[12:34:03] MODE: Manual\n" +
                        "[12:34:05] ATTITUDE roll=2.3° pitch=-1.4°\n" +
                        "[12:34:06] BATTERY 85% 14.8V\n" +
                        "[12:34:07] DEPTH 5.4m\n" +
                        "[12:34:08] HEADING 15.4°\n" +
                        "[12:34:09] THROTTLE 50%\n" +
                        "[12:34:10] LINK OK\n" +
                        "[12:34:11] ATTITUDE roll=3.1° pitch=-1.2°\n" +
                        "[12:34:12] GPS HDOP 0.8\n" +
                        "[12:34:13] BATTERY 84% 14.7V\n" +
                        "[12:34:14] DEPTH 5.5m\n" +
                        "[12:34:15] HEARTBEAT received sysid=1\n" +
                        "[12:34:16] ATTITUDE roll=2.8° pitch=-1.5°\n" +
                        "[12:34:17] BATTERY 84% 14.7V\n" +
                        "[12:34:18] DEPTH 5.6m\n" +
                        "[12:34:19] HEADING 16.1°\n"
                }

                // 오른쪽 끝 세로 스크롤바
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOn
                    width: 6
                    anchors.right: parent.right
                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 3
                        color: "#6a7782"
                    }
                    background: Rectangle {
                        implicitWidth: 6
                        color: "#1a2530"
                        radius: 3
                    }
                }
            }
        }

        // 우측 절반: 컨트롤 센터
        Rectangle {
            id: controlCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.left: logCard.right
            anchors.leftMargin: 12
            radius: 12
            color: "#B31a2530"
            border.color: "#2a3540"
            border.width: 1
            antialiasing: true
            layer.enabled: true
            layer.samples: 8

            Text {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 10
                anchors.leftMargin: 14
                text: "CONTROL CENTER"
                color: "#8faabc"
                font.pixelSize: 10
                font.letterSpacing: 1.2
            }

            // ── UUV 그림 (컨트롤 센터 전체 높이) ──────────────────
            Image {
                id: uuvImage
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.topMargin: 18
                anchors.bottomMargin: 0
                width: 190
                source: "qrc:/assets/UUV.png"
                sourceSize.width: 380
                sourceSize.height: 280
                smooth: true
                antialiasing: true
                mipmap: true
                fillMode: Image.PreserveAspectFit
            }

            // 한 줄로 정렬: [텔레메트리] [조이스틱 토글]
            Row {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: uuvImage.right
                anchors.right: parent.right
                anchors.topMargin: 28
                anchors.bottomMargin: 12
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 14

                // ── 텔레메트리 (배터리 → 속도 → 스로틀 가로 배치) ──
                Row {
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 28

                    // 배터리
                    Row {
                        height: 40
                        spacing: 10
                        Rectangle {
                            width: 40; height: 40; radius: 14
                            color: "#0d1620"
                            anchors.verticalCenter: parent.verticalCenter
                            antialiasing: true
                            Item {
                                anchors.centerIn: parent
                                width: 22; height: 12
                                Rectangle {
                                    id: batShellInner
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 18; height: 12
                                    radius: 2
                                    color: "transparent"
                                    border.color: "#eeeeee"
                                    border.width: 1.2
                                    antialiasing: true
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 2
                                        width: (parent.width - 4) * 0.85
                                        color: "#eeeeee"
                                        radius: 1
                                    }
                                }
                                Rectangle {
                                    anchors.left: batShellInner.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 2; height: 5
                                    color: "#eeeeee"
                                    radius: 1
                                }
                            }
                        }
                        Text {
                            text: "16.3hr"
                            color: "#cccccc"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // 속도
                    Row {
                        height: 40
                        spacing: 10
                        Rectangle {
                            width: 40; height: 40; radius: 14
                            color: "#0d1620"
                            anchors.verticalCenter: parent.verticalCenter
                            antialiasing: true
                            // 속도계 아이콘 (반원 + 바늘)
                            Canvas {
                                anchors.centerIn: parent
                                width: 26; height: 28
                                Component.onCompleted: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = "#eeeeee"
                                    ctx.lineWidth = 1.5
                                    var cx = width / 2
                                    var cy = height - 6
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, 10, Math.PI, 0)
                                    ctx.stroke()
                                    ctx.beginPath()
                                    ctx.moveTo(cx, cy)
                                    ctx.lineTo(cx + 7, cy - 8)
                                    ctx.stroke()
                                }
                            }
                        }
                        Text {
                            text: "13 km/h"
                            color: "#cccccc"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // 스로틀
                    Row {
                        height: 40
                        spacing: 10
                        Rectangle {
                            width: 40; height: 40; radius: 14
                            color: "#0d1620"
                            anchors.verticalCenter: parent.verticalCenter
                            antialiasing: true
                            // 스로틀 아이콘 (수직 막대 게이지)
                            Item {
                                anchors.centerIn: parent
                                width: 18; height: 22

                                // 외곽 프레임
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.color: "#eeeeee"
                                    border.width: 1.2
                                    radius: 3
                                    antialiasing: true
                                }
                                // 채움 (50% 가정)
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 2
                                    height: (parent.height - 4) * 0.5
                                    color: "#eeeeee"
                                    radius: 1
                                }
                            }
                        }
                        Text {
                            text: "50 %"
                            color: "#cccccc"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

            }

            // ── 조이스틱 토글 버튼 (컨트롤 센터 우상단, 정사각형) ────
            Rectangle {
                id: joystickToggleBtn
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 8
                anchors.rightMargin: 8
                width: 80; height: 80
                radius: 12
                color: jsToggleMa.containsMouse ? "#B3243341" : "#1f2a35"
                border.color: "#2a3540"
                border.width: 1
                antialiasing: true

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 32; height: 32; radius: 16
                        color: "#0d1620"
                        border.color: joystickVisible ? "#61d3ff" : "#4a5560"
                        border.width: 1.5
                        antialiasing: true
                        Rectangle {
                            anchors.centerIn: parent
                            width: 12; height: 12; radius: 6
                            color: joystickVisible ? "#61d3ff" : "#5a6770"
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: joystickVisible ? "ON" : "OFF"
                        color: joystickVisible ? "#61d3ff" : "#8faabc"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.0
                    }
                }

                MouseArea {
                    id: jsToggleMa
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: joystickVisible = !joystickVisible
                }
            }
        }
    }
}
