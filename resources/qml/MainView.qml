import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12
import QtLocation 5.12
import QtPositioning 5.12
import QtQuick.Scene3D 2.0
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Extras 2.0
import Qt3D.Input 2.0

Rectangle {
    id: root
    anchors.fill: parent
    color: "#0d1620"

    // 맵 모드: "osm" / "voyager" / "3d"
    // onMapModeChanged 핸들러는 아래 Loader 블록에서 통합 처리
    // (latch 플래그 갱신 + bridge 알림).
    property string mapMode: "osm"

    // 두 맵이 공유하는 center/zoom (토글 시 위치/줌 유지)
    property var mapCenter: QtPositioning.coordinate(37.52951029463262, 126.94149832867085)
    property real mapZoom: 15

    // MAVLink GPS 첫 수신 시 맵 중앙을 로봇 위치로 부드럽게 이동 (sysid당 1회)
    property bool _gpsCentered: false

    // ease-out cubic 보간 타이머 — mapCenter 직접 갱신 (16ms ≈ 60fps)
    Timer {
        id: gpsPanTimer
        interval: 16
        repeat: true
        property real fromLat: 0; property real fromLon: 0
        property real toLat:   0; property real toLon:   0
        property int  elapsed: 0
        readonly property int duration: 1400

        onTriggered: {
            elapsed += interval
            var t = Math.min(elapsed / duration, 1.0)
            t = 1 - Math.pow(1 - t, 3)   // ease-out cubic
            mapCenter = QtPositioning.coordinate(
                fromLat + (toLat - fromLat) * t,
                fromLon + (toLon - fromLon) * t)
            if (elapsed >= duration) { stop(); elapsed = 0 }
        }

        function panTo(lat, lon) {
            fromLat = mapCenter.latitude;  fromLon = mapCenter.longitude
            toLat   = lat;                 toLon   = lon
            elapsed = 0
            restart()
        }
    }

    Connections {
        target: vehicle
        function onGpsChanged() {
            if (_gpsCentered || !vehicle) return
            var lat = vehicle.latitude, lon = vehicle.longitude
            if (lat === 0 && lon === 0) return
            gpsPanTimer.panTo(lat, lon)
            _gpsCentered = true
        }
        // 활성 차량이 바뀌면 latch 해제 → 새 차량 GPS 오면 다시 이동
        function onSysidChanged() { _gpsCentered = false }
    }

    // ── 모드별 Loader: 한 번 로드되면 unload하지 않고 visible만 토글 ──
    // 모드 전환 시 컴포넌트 destroy/recreate를 피해서:
    //   1. 카메라 위치/지도 줌 상태 보존
    //   2. 3D entity 트리 dangling 방지 (이전 세그폴트 원인)
    //   3. 타일 재fetch 방지
    // 메모리 trade-off: 처음 들어간 모드의 컴포넌트가 계속 메모리에 남는다.

    Loader {
        id: osmLoader
        anchors.fill: parent
        sourceComponent: osmComponent
        active: true                       // 디폴트 모드라 시작부터 로드
        visible: mapMode === "osm"
    }

    Loader {
        id: voyagerLoader
        anchors.fill: parent
        sourceComponent: voyagerComponent
        active: mapMode === "voyager" || _voyagerEverActive
        visible: mapMode === "voyager"
    }

    Loader {
        id: terrain3dLoader
        anchors.fill: parent
        sourceComponent: terrain3dComponent
        active: mapMode === "3d" || _terrain3dEverActive
        visible: mapMode === "3d"
    }

    // 한 번이라도 active 였는지 추적 → 다시 false로 안 떨어지게 latch
    property bool _voyagerEverActive: false
    property bool _terrain3dEverActive: false
    onMapModeChanged: {
        if (mapMode === "voyager") _voyagerEverActive = true
        if (mapMode === "3d")      _terrain3dEverActive = true
        if (bridge) bridge.setMapMode(mapMode)
    }

    // 3D 모드: Scene3D에 C++ TerrainScene의 entity 트리를 reparent하여 렌더링.
    // QML 자체 합성이므로 카드/로그/조이스틱 등 다른 컨트롤이 자연스럽게 위에 보임.
    Component {
        id: terrain3dComponent
        Scene3D {
            id: terrainScene3d
            aspects: ["input", "logic"]
            cameraAspectRatioMode: Scene3D.AutomaticAspectRatio
            multisample: true
            focus: true

            Entity {
                id: scene3dRoot

                Camera {
                    id: terrainCam
                    projectionType: CameraLens.PerspectiveProjection
                    fieldOfView: 60
                    nearPlane: 0.1
                    farPlane: 10000
                    position: Qt.vector3d(0, 179, 128)
                    upVector: Qt.vector3d(0, 1, 0)
                    viewCenter: Qt.vector3d(0, 0, 0)
                }

                OrbitCameraController {
                    camera: terrainCam
                    linearSpeed: -400
                    lookSpeed: -180
                }

                components: [
                    RenderSettings {
                        activeFrameGraph: ForwardRenderer {
                            clearColor: "#0f121e"
                            camera: terrainCam
                        }
                    },
                    InputSettings {}
                ]

                // 방향 조명
                Entity {
                    components: [
                        DirectionalLight {
                            worldDirection: Qt.vector3d(-0.3, -1.0, -0.5)
                            color: "white"
                            intensity: 1.5
                        }
                    ]
                }

                Component.onCompleted: {
                    if (mainTerrainScene) {
                        mainTerrainScene.setCamera(terrainCam)
                        // attachTo: 캐시 있으면 즉시 rebuild, GPS fix 있으면 로봇 위치로 fetch
                        mainTerrainScene.attachTo(scene3dRoot)
                        // GPS도 캐시도 없을 때만 기본 좌표로 fetch
                        if (!mainTerrainScene.hasTerrainData()) {
                            var lat = (vehicle && vehicle.latitude  !== 0) ? vehicle.latitude  : 37.52951029463262
                            var lon = (vehicle && vehicle.longitude !== 0) ? vehicle.longitude : 126.94149832867085
                            mainTerrainScene.loadTile(lat, lon, 17)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: osmComponent
        Map {
            id: osmMap
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

            // 양방향 바인딩 루프 방지:
            //   center: mapCenter  +  onCenterChanged: mapCenter = center → QML이 loop 감지
            // 초기값은 onCompleted에서 한 번만 세팅, 이후엔 user gesture → mapCenter 단방향.
            onCenterChanged:    if (mapCenter !== center)  mapCenter = center
            onZoomLevelChanged: if (mapZoom   !== zoomLevel) mapZoom   = zoomLevel

            Component.onCompleted: {
                center    = mapCenter
                zoomLevel = mapZoom
                for (var i = 0; i < supportedMapTypes.length; i++) {
                    if (supportedMapTypes[i].style === MapType.CustomMap) {
                        activeMapType = supportedMapTypes[i]
                        break
                    }
                }
            }
            // 외부에서 mapCenter/mapZoom이 변경되면 (다른 모드에서 갱신된 값)
            // 우리도 따라가되, 같은 값이면 set 안 해서 루프 방지.
            Connections {
                target: root
                function onMapCenterChanged() { if (osmMap.center !== mapCenter) osmMap.center = mapCenter }
                function onMapZoomChanged()   { if (osmMap.zoomLevel !== mapZoom) osmMap.zoomLevel = mapZoom }
            }

            // 다중로봇: 감지된 모든 차량을 마커로 표시. 활성(선택) 차량만 불투명, 나머지는 흐리게.
            MapItemView {
                model: vehicleManager
                delegate: MapQuickItem {
                    visible: model.vehicle && (model.vehicle.latitude !== 0 || model.vehicle.longitude !== 0)
                    coordinate: QtPositioning.coordinate(
                        model.vehicle ? model.vehicle.latitude  : 0,
                        model.vehicle ? model.vehicle.longitude : 0)
                    anchorPoint.x: markerImgOsm.width  / 2
                    anchorPoint.y: markerImgOsm.height / 2
                    sourceItem: Image {
                        id: markerImgOsm
                        source: "qrc:/assets/sub.png"
                        width: 70; height: 70
                        sourceSize.width: 140; sourceSize.height: 140
                        fillMode: Image.PreserveAspectFit
                        smooth: true; antialiasing: true; mipmap: true
                        opacity: (vehicleManager && model.sysid === vehicleManager.activeSysid) ? 1.0 : 0.45
                        transform: Rotation {
                            origin.x: markerImgOsm.width  / 2
                            origin.y: markerImgOsm.height / 2
                            // +90°: DAVE yaw 프레임 보정
                            angle: model.vehicle ? model.vehicle.yaw * 180 / Math.PI + 90 : 0
                        }
                    }
                }
            }
        }
    }

    Component {
        id: voyagerComponent
        Map {
            id: voyagerMap
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

            onCenterChanged:    if (mapCenter !== center)  mapCenter = center
            onZoomLevelChanged: if (mapZoom   !== zoomLevel) mapZoom   = zoomLevel

            Component.onCompleted: {
                center    = mapCenter
                zoomLevel = mapZoom
                for (var i = 0; i < supportedMapTypes.length; i++) {
                    if (supportedMapTypes[i].style === MapType.CustomMap) {
                        activeMapType = supportedMapTypes[i]
                        break
                    }
                }
            }
            Connections {
                target: root
                function onMapCenterChanged() { if (voyagerMap.center !== mapCenter) voyagerMap.center = mapCenter }
                function onMapZoomChanged()   { if (voyagerMap.zoomLevel !== mapZoom) voyagerMap.zoomLevel = mapZoom }
            }

            // 다중로봇: 감지된 모든 차량을 마커로 표시. 활성(선택) 차량만 불투명, 나머지는 흐리게.
            MapItemView {
                model: vehicleManager
                delegate: MapQuickItem {
                    visible: model.vehicle && (model.vehicle.latitude !== 0 || model.vehicle.longitude !== 0)
                    coordinate: QtPositioning.coordinate(
                        model.vehicle ? model.vehicle.latitude  : 0,
                        model.vehicle ? model.vehicle.longitude : 0)
                    anchorPoint.x: markerImgVoyager.width  / 2
                    anchorPoint.y: markerImgVoyager.height / 2
                    sourceItem: Image {
                        id: markerImgVoyager
                        source: "qrc:/assets/sub.png"
                        width: 70; height: 70
                        sourceSize.width: 140; sourceSize.height: 140
                        fillMode: Image.PreserveAspectFit
                        smooth: true; antialiasing: true; mipmap: true
                        opacity: (vehicleManager && model.sysid === vehicleManager.activeSysid) ? 1.0 : 0.45
                        transform: Rotation {
                            origin.x: markerImgVoyager.width  / 2
                            origin.y: markerImgVoyager.height / 2
                            // +90°: DAVE yaw 프레임 보정
                            angle: model.vehicle ? model.vehicle.yaw * 180 / Math.PI + 90 : 0
                        }
                    }
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

    // 저장된 연결 엔트리 목록 (host:port). 사용자가 Add Connection으로 추가.
    // 실제 연결 상태는 connection 브리지(C++)가 관리하고 QML은 mirror만 함.
    // 한 번에 한 연결만 active (Phase 1: LinkManager single-link).
    ListModel {
        id: connectionsModel
        // 항목 스키마: { host: string, port: int, type: string ("UDP"/"TCP") }
    }

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
                // 활성 sysid가 0이면 미정 — 연결 안 됐거나 HEARTBEAT 대기 중.
                // 0이 아니면 실제 sysid 표시. VehicleState.sysid는 active sysid를 mirror.
                text: {
                    if (!vehicle || vehicle.sysid === 0) {
                        if (connection && connection.connected) return "sys_id : detecting…"
                        return "sys_id : -"
                    }
                    return "sys_id: " + vehicle.sysid
                }
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
            // ── 동적 폭 ─────────────────────────────────────────────
            //   1) 비히클 카드: cardWidth 고정, 한 줄 최대 5개. 감지된 수만큼 grid 폭 결정.
            //   2) 연결 행: "type host:port" 텍스트 + Connect 버튼 폭 + 마진.
            //   둘 중 큰 값 + minBaseWidth 와 비교해 popup width 결정.
            property int minBaseWidth:   240                 // Add Connection 등 최소 폭
            property int cardWidth:      90
            property int gridSidePad:    24                  // grid 좌우 padding (x:12 양쪽)
            property int gridCellGap:    8
            property int vehiclesCount:  connection ? connection.detectedSysids.length : 0
            property int gridColumns:    Math.min(Math.max(vehiclesCount, 1), 5)
            property int vehiclesWidth:  gridSidePad + gridColumns * cardWidth
                                          + (gridColumns - 1) * gridCellGap
            property int connRowExtra:   14 + 12 + 84 + 10 + 6   // leftMargin + gap + btn + rightMargin + 여유
            property int maxConnRowWidth: 0

            width: Math.max(minBaseWidth, vehiclesWidth, maxConnRowWidth)
            padding: 0
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape

            // 연결 행 텍스트 폭 측정용
            TextMetrics {
                id: connTextMetrics
                font.pixelSize: 12
                font.family: "monospace"
            }

            function recalcMaxRowWidth() {
                var maxW = 0
                for (var i = 0; i < connectionsModel.count; i++) {
                    var it = connectionsModel.get(i)
                    connTextMetrics.text = it.type.toLowerCase() + " " + it.host + ":" + it.port
                    var w = connTextMetrics.advanceWidth + connRowExtra
                    if (w > maxW) maxW = w
                }
                maxConnRowWidth = maxW
            }

            Connections {
                target: connectionsModel
                function onCountChanged() { sysIdPopup.recalcMaxRowWidth() }
            }
            Component.onCompleted: recalcMaxRowWidth()

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

                // ── CONNECTIONS 섹션 ──────────────────────────────
                Text {
                    padding: 14
                    text: "CONNECTIONS"
                    color: "#8faabc"
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                }

                // 연결 리스트 — 각 엔트리는 host:port + Connect/Disconnect 버튼
                Repeater {
                    model: connectionsModel
                    Rectangle {
                        width: sysIdPopup.width
                        height: 40
                        // 이 엔트리가 현재 활성 연결인지 — host:port 일치 + connected
                        property bool isActive: connection
                            && connection.connected
                            && connection.currentHost === host
                            && connection.currentPort === port
                        color: connRowMa.containsMouse ? "#B3243341" : "transparent"

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 14
                            text: type.toLowerCase() + " " + host + ":" + port
                            color: "#cccccc"
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                        // Connect/Disconnect 버튼
                        Rectangle {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: 10
                            width: 84; height: 24
                            radius: 8
                            color: connBtnMa.containsMouse
                                ? (parent.isActive ? "#c93030" : "#3eaedb")
                                : (parent.isActive ? "#a82424" : "#61d3ff")
                            antialiasing: true
                            Text {
                                anchors.centerIn: parent
                                text: parent.parent.isActive ? "Disconnect" : "Connect"
                                color: parent.parent.isActive ? "#ffffff" : "#0d1620"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                            MouseArea {
                                id: connBtnMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (parent.parent.isActive) connection.disconnectLink()
                                    else connection.connectUdp(host, port)
                                }
                            }
                        }
                        MouseArea {
                            id: connRowMa
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton  // 버튼 클릭 우회용
                        }
                    }
                }

                // + Add Connection (CONNECTIONS 섹션 끝)
                Rectangle {
                    width: sysIdPopup.width
                    height: 40
                    color: addMa.containsMouse ? "#B3243341" : "transparent"

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        verticalAlignment: Text.AlignVCenter
                        text: "＋  Add Connection"
                        color: "#61d3ff"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: addMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: addVehicleDialog.open()
                    }
                }

                Rectangle {
                    width: sysIdPopup.width
                    height: 1
                    color: "#2a3540"
                }

                // ── VEHICLES 섹션 (mock 데이터 5열 그리드) ──────────
                Text {
                    padding: 14
                    text: "VEHICLES"
                    color: "#8faabc"
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                }

                Grid {
                    x: 12
                    width: sysIdPopup.width - 24
                    columns: 5
                    rowSpacing: 8
                    columnSpacing: 8
                    bottomPadding: 12

                    // 감지된 sysid가 없으면 안내 메시지
                    Text {
                        visible: !connection || connection.detectedSysids.length === 0
                        width: sysIdPopup.width - 24
                        topPadding: 10
                        bottomPadding: 10
                        horizontalAlignment: Text.AlignHCenter
                        text: "no vehicles — connect to a link first"
                        color: "#5a6a78"
                        font.pixelSize: 11
                        font.italic: true
                    }

                    Repeater {
                        // ConnectionBridge.vehiclesInfo — 모든 감지된 차량의 실시간 슬롯
                        model: connection ? connection.vehiclesInfo : []

                        delegate: Rectangle {
                            // 카드 폭은 고정 — popup width가 카드 수에 맞춰 늘었다 줄었다 함
                            width: sysIdPopup.cardWidth
                            height: 170
                            radius: 8

                            property int  cardSysid:   modelData.sysid
                            property bool isActive:    connection && connection.activeSysid === cardSysid
                            property int  cardBattery: modelData.batteryRemaining
                            property int  cardSignal:  modelData.signalLevel

                            color: cardMa.containsMouse ? "#152332" : "#0d1620"
                            border.color: isActive ? "#aaff00" : "#2a3540"
                            border.width: isActive ? 2 : 1
                            antialiasing: true

                            Column {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 8

                                // AUV 이미지 — 가로 폭에 맞춰 채움
                                Image {
                                    width: parent.width
                                    height: 52
                                    source: "qrc:/assets/AUV.png"
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                    mipmap: true
                                    sourceSize.width: 220
                                    sourceSize.height: 220
                                }

                                // WiFi 아이콘 (Canvas) — 흰색, 1.5배 키움
                                Canvas {
                                    id: wifiCanvas
                                    width: parent.width
                                    height: 33
                                    property int level: cardSignal
                                    onLevelChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, width, height)
                                        var cx = width / 2
                                        var cy = height - 3
                                        var activeColor   = "#ffffff"
                                        var inactiveColor = "#3a4a55"
                                        for (var i = 1; i <= 3; i++) {
                                            ctx.strokeStyle = i <= level ? activeColor : inactiveColor
                                            ctx.lineWidth = 3.0
                                            ctx.beginPath()
                                            var r = 0.6 + i * 3.9   // 비례 1.5배
                                            ctx.arc(cx, cy, r, Math.PI * 1.25, Math.PI * 1.75)
                                            ctx.stroke()
                                        }
                                        ctx.fillStyle = level > 0 ? activeColor : inactiveColor
                                        ctx.beginPath()
                                        ctx.arc(cx, cy, 1.5, 0, Math.PI * 2)
                                        ctx.fill()
                                    }
                                    Component.onCompleted: requestPaint()
                                }

                                // 배터리 인디케이터
                                Row {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    spacing: 5

                                    Item {
                                        width: 28; height: 12
                                        anchors.verticalCenter: parent.verticalCenter

                                        Rectangle {
                                            id: cardBatShell
                                            anchors.left: parent.left
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 25; height: 12
                                            radius: 2.5
                                            color: "#0d1620"
                                            border.color: "#8faabc"
                                            border.width: 1
                                            antialiasing: true

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                anchors.margins: 1.6
                                                radius: 1.2
                                                width: cardBattery < 0
                                                    ? 0
                                                    : (cardBatShell.width - 3.2) * cardBattery / 100
                                                color: {
                                                    if (cardBattery < 0)  return "#3a4a55"
                                                    if (cardBattery < 20) return "#ff5252"
                                                    if (cardBattery < 50) return "#ffb347"
                                                    return "#aaff00"
                                                }
                                                antialiasing: true
                                            }
                                        }
                                        Rectangle {
                                            anchors.left: cardBatShell.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 2.5; height: 5
                                            radius: 1
                                            color: "#8faabc"
                                        }
                                    }

                                    Text {
                                        text: cardBattery < 0 ? "—" : (cardBattery + "%")
                                        color: "#cccccc"
                                        font.pixelSize: 11
                                        font.family: "monospace"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                // sysid 라벨
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "sysid : " + cardSysid
                                    color: "#cccccc"
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    font.weight: Font.DemiBold
                                }
                            }

                            MouseArea {
                                id: cardMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: if (connection) connection.setActiveSysid(cardSysid)
                            }
                        }
                    }
                }
                }   // Column 닫기
            }       // 래핑 Item 닫기
        }
    }

    // ── Add Connection 모달 다이얼로그 ──────────────────────────────
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
                text: "Add Connection"
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
                    Text { anchors.centerIn: parent; text: "Add"; color: "#0d1620"; font.pixelSize: 12; font.weight: Font.DemiBold }
                    MouseArea {
                        id: connectMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            var h = hostField.text
                            var p = parseInt(portField.text)
                            // 중복 체크
                            for (var i = 0; i < connectionsModel.count; i++) {
                                var it = connectionsModel.get(i)
                                if (it.host === h && it.port === p) {
                                    dupToast.show()
                                    return
                                }
                            }
                            connectionsModel.append({ host: h, port: p, type: addVehicleDialog.connType })
                            addVehicleDialog.close()
                            sysIdPopup.open()
                        }
                    }
                }
            }
        }
    }

    // 중복 연결 토스트
    Rectangle {
        id: dupToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 230
        width: dupToastText.implicitWidth + 32
        height: 36
        radius: 10
        color: "#cc2a3540"
        border.color: "#ff5252"
        border.width: 1
        opacity: 0
        z: 100

        Text {
            id: dupToastText
            anchors.centerIn: parent
            text: "Already exists"
            color: "#ff5252"
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        function show() {
            opacity = 1
            hideTimer.restart()
        }

        Timer {
            id: hideTimer
            interval: 2000
            onTriggered: dupToast.opacity = 0
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    // SysID 우측: 링크 상태 카드 (텍스트 길이에 따라 가변폭)
    Rectangle {
        id: linkCard
        property bool linkOk: vehicle ? vehicle.heartbeatOk : false

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
        property bool armed: vehicle ? vehicle.armed : false

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
            // GPS 좌표. monospace + 공백 패딩으로 ° N, ° E 위치 고정.
            // 위도: "XX.XXXXXXX" 10자리, 경도: "XXX.XXXXXXX" 11자리 우측 정렬.
            text: {
                if (!vehicle) return "—"
                function pad(s, n) { while (s.length < n) s = " " + s; return s }
                var lat = vehicle.latitude, lon = vehicle.longitude
                if (lat === 0 && lon === 0)
                    return pad("---", 10) + "° N,  " + pad("---", 11) + "° E"
                return pad(Math.abs(lat).toFixed(7), 10) + "° " + (lat >= 0 ? "N" : "S")
                     + ",  " + pad(Math.abs(lon).toFixed(7), 11) + "° " + (lon >= 0 ? "E" : "W")
            }
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
        heading: vehicle ? vehicle.heading : 0
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
            onClicked: mapModePopup.opened ? mapModePopup.close() : mapModePopup.open()
        }

        // 맵 모드 선택 팝업 (OSM / Voyager / 3D)
        Popup {
            id: mapModePopup
            x: -width + mapToggleBtn.width
            y: mapToggleBtn.height + 6
            width: 140
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
                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Rectangle {
                        width: mapModePopup.width
                        height: mapModePopup.height
                        radius: 12
                    }
                }

                Column {
                    anchors.fill: parent
                    spacing: 0

                    Text {
                        padding: 12
                        text: "MODE"
                        color: "#8faabc"
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                    }

                    Repeater {
                        model: [
                            { key: "osm",     label: "Dark"   },
                            { key: "voyager", label: "Bright" },
                            { key: "3d",      label: "3D"     }
                        ]
                        delegate: Rectangle {
                            width: mapModePopup.width
                            height: 36
                            color: modeMa.containsMouse ? "#B3243341" : "transparent"

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 6
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: mapMode === modelData.key ? "✓" : "  "
                                    color: "#44bb44"
                                    font.pixelSize: 12
                                    width: 12
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: "#cccccc"
                                    font.pixelSize: 12
                                }
                            }

                            MouseArea {
                                id: modeMa
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    mapMode = modelData.key
                                    mapModePopup.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 줌 컨트롤 (지도 토글 카드 밑) ──────────────────────────────
    // +  : zoom +1
    // 표시 : 현재 줌 (0.1 단위 사용 가능)
    // -  : zoom -1
    // 입력 : 직접 값 입력 (0.1 단위)
    Column {
        id: zoomControl
        visible: mapMode !== "3d"
        anchors.bottom: depthGauge.top
        anchors.right: parent.right
        anchors.bottomMargin: 22
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
                    Text {
                        // 위성 3개 이상 + 좌표 != 0 이면 3D fix로 본다 (간단 판단)
                        text: {
                            if (!vehicle) return "Fix:         —"
                            var fix = (vehicle.gpsSatCount >= 3 && (vehicle.latitude !== 0 || vehicle.longitude !== 0))
                                      ? "3D" : "No fix"
                            return "Fix:         " + fix
                        }
                        color: "#cccccc"; font.pixelSize: 12; font.family: "monospace"
                    }
                    Text {
                        text: "Satellites:  " + (vehicle ? vehicle.gpsSatCount : 0)
                        color: "#cccccc"; font.pixelSize: 12; font.family: "monospace"
                    }
                    Text {
                        text: "HDOP:        " + (vehicle ? vehicle.gpsHdop.toFixed(1) : "—")
                        color: "#cccccc"; font.pixelSize: 12; font.family: "monospace"
                    }
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
    // Operations 탭 JoystickWidget과 동일한 축 매핑/deadband.
    // 각 패드의 정규화 (x, y)를 root.leftX/leftY/rightX/rightY로 보관 →
    // 아래 manualControlTimer가 50Hz로 MANUAL_CONTROL 패킷 발신.
    property real leftX:  0
    property real leftY:  0
    property real rightX: 0
    property real rightY: 0

    MainStickPad {
        id: leftStickPad
        visible: joystickVisible
        anchors.bottom: bottomBar.top
        anchors.left: bottomBar.left
        anchors.bottomMargin: 12
        anchors.leftMargin: 60
        width: 110; height: 110
        labelText: "Strafe / Fwd"
        onStickChanged: { root.leftX = x; root.leftY = y }
    }

    // 50Hz로 MANUAL_CONTROL 송신 — Operations 탭 JoystickWidget과 동일 주기.
    // 연결됐을 때만 실행. 스틱이 중립이어도 계속 보내야 ArduSub이 GCS 연결로 인식.
    Timer {
        id: manualControlTimer
        interval: 20
        repeat: true
        running: connection && connection.connected
        onTriggered: {
            if (!commander) return
            // 축 매핑 (JoystickWidget.h 주석 그대로):
            //   x = -axes[1]*1000 = -leftY*1000  (위로 밀면 전진)
            //   y = -axes[0]*1000 =  leftX*1000  (오른쪽 = 우측 strafe)
            //   z = (1 - axes[5]) / 2 * 1000      (위=상승, 중립=500)
            //   r = -axes[4]*1000 =  rightX*1000  (오른쪽 = 우회전)
            var mcX = Math.round(-leftY  * 1000)
            var mcY = Math.round( leftX  * 1000)
            var mcZ = Math.round((1 - rightY) / 2 * 1000)
            var mcR = Math.round( rightX * 1000)
            commander.sendManualControl(mcX, mcY, mcZ, mcR, 0)
        }
    }

    AttitudeIndicator {
        id: attitudeGauge
        anchors.bottom: bottomBar.top
        anchors.right: parent.right
        anchors.bottomMargin: 12
        anchors.rightMargin: 16
        width: 170; height: 170
        // VehicleState는 라디안으로 저장 → 도(°) 변환
        rollDeg:  vehicle ? vehicle.roll  * 180 / Math.PI : 0
        pitchDeg: vehicle ? vehicle.pitch * 180 / Math.PI : 0
    }

    // 수심 게이지 (롤/피치 게이지 위, 동일 우측 마진)
    // VFR_HUD.alt는 해수면 기준 음수 (수중일 때) — 게이지는 양수 magnitude를 받음
    DepthGauge {
        id: depthGauge
        anchors.bottom: attitudeGauge.top
        anchors.right: parent.right
        anchors.bottomMargin: 12
        anchors.rightMargin: 2
        width: 100; height: 160
        depth: vehicle ? Math.max(0, -vehicle.depth) : 0
        maxDepth: 100
    }

    MainStickPad {
        id: rightStickPad
        visible: joystickVisible
        anchors.bottom: bottomBar.top
        anchors.right: attitudeGauge.left
        anchors.rightMargin: 50
        anchors.bottomMargin: 12
        width: 110; height: 110
        labelText: "Yaw / Heave"
        onStickChanged: { root.rightX = x; root.rightY = y }
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
                    text: logFeed ? logFeed.text : ""

                    // 새 로그가 추가되면 항상 맨 아래로 스크롤
                    // contentHeight가 새 텍스트로 확정된 다음 프레임에 이동해야 하므로 Qt.callLater 사용
                    onTextChanged: Qt.callLater(_scrollToBottom)
                    function _scrollToBottom() {
                        logFlick.contentY = Math.max(0, logFlick.contentHeight - logFlick.height)
                    }
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
                anchors.topMargin: 6
                anchors.leftMargin: 14
                text: "CONTROL CENTER"
                color: "#8faabc"
                font.pixelSize: 10
                font.letterSpacing: 1.2
            }

            // 활성 차량 없을 때 안내 (카드를 클릭해야 조종 콘텐츠가 뜸)
            Text {
                anchors.centerIn: parent
                visible: !vehicle || vehicle.sysid === 0
                text: "Select a vehicle from VEHICLES"
                color: "#5a6a78"
                font.pixelSize: 13
                font.italic: true
            }

            // ── UUV 그림 (컨트롤 센터 전체 높이) ──────────────────
            Image {
                id: uuvImage
                visible: vehicle && vehicle.sysid > 0
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

            // 텔레메트리 영역 (UUV 그림 ↔ 컨트롤 패널 사이를 차지)
            Item {
                visible: vehicle && vehicle.sysid > 0
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: uuvImage.right
                anchors.right: controlPanel.left
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                anchors.leftMargin: 14
                anchors.rightMargin: 12

                // ── 텔레메트리 (배터리 → 속도 → 스로틀 세로 배치) — 영역 중앙 정렬 ──
                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    // 배터리
                    Row {
                        height: 36
                        spacing: 8

                        // 배터리 아이콘 (가로 바 스타일) — 시작점을 다른 Row와 통일 (width 36)
                        Item {
                            width: 36; height: 14
                            anchors.verticalCenter: parent.verticalCenter

                            // 외곽 셸 — Item 중앙에 배치
                            Rectangle {
                                id: batShell
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.horizontalCenterOffset: -2  // 단자 폭만큼 좌측으로
                                anchors.verticalCenter: parent.verticalCenter
                                width: 30; height: 13
                                radius: 3
                                color: "#0d1620"
                                border.color: "#8faabc"
                                border.width: 1.2
                                antialiasing: true

                                // 채움 바
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 2
                                    radius: 2
                                    width: {
                                        var pct = (vehicle && vehicle.batteryRemaining >= 0)
                                                  ? Math.max(0, Math.min(100, vehicle.batteryRemaining)) : 0
                                        return (batShell.width - 4) * pct / 100
                                    }
                                    color: {
                                        if (!vehicle || vehicle.batteryRemaining < 0) return "#3a4a55"
                                        if (vehicle.batteryRemaining < 20) return "#ff5252"
                                        if (vehicle.batteryRemaining < 50) return "#ffb347"
                                        return "#aaff00"
                                    }
                                    antialiasing: true
                                }
                            }
                            // 양극 단자 (오른쪽 돌출)
                            Rectangle {
                                anchors.left: batShell.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 0
                                width: 3; height: 6
                                radius: 1
                                color: "#8faabc"
                            }
                        }

                        Text {
                            text: {
                                if (!vehicle || vehicle.batteryRemaining < 0) return "—%"
                                return vehicle.batteryRemaining + "%  " + vehicle.voltage.toFixed(1) + "V"
                            }
                            color: "#cccccc"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // 속도 — velocity.png 아이콘 + 수치
                    Row {
                        height: 36
                        spacing: 8
                        Image {
                            width: 36; height: 36
                            source: "qrc:/assets/velocity.png"
                            sourceSize.width: 144
                            sourceSize.height: 144
                            smooth: true
                            antialiasing: true
                            mipmap: true
                            fillMode: Image.PreserveAspectFit
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            // VFR_HUD.groundspeed는 m/s — km/h로 변환
                            text: vehicle
                                ? (vehicle.groundspeed * 3.6).toFixed(1) + " km/h"
                                : "— km/h"
                            color: "#cccccc"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // 스로틀 — 원형 프로그래스바
                    Row {
                        height: 36
                        spacing: 8
                        Canvas {
                            id: throttleRing
                            width: 36; height: 36
                            anchors.verticalCenter: parent.verticalCenter
                            antialiasing: true
                            // ArduSub 음수(역방향) throttle을 magnitude로 표시
                            property real pct: vehicle
                                ? Math.max(0, Math.min(100, Math.abs(vehicle.throttle))) / 100
                                : 0
                            onPctChanged: requestPaint()
                            Component.onCompleted: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2
                                var cy = height / 2
                                var r  = 14
                                // 배경 트랙
                                ctx.strokeStyle = "#2a3540"
                                ctx.lineWidth = 4
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                                ctx.stroke()
                                // 진행 호 (12시부터 시계방향)
                                if (pct > 0) {
                                    ctx.strokeStyle = "#aaff00"
                                    ctx.lineWidth = 4
                                    ctx.lineCap = "round"
                                    ctx.beginPath()
                                    var start = -Math.PI / 2
                                    ctx.arc(cx, cy, r, start, start + Math.PI * 2 * pct)
                                    ctx.stroke()
                                }
                            }
                        }
                        Text {
                            text: (vehicle ? Math.abs(vehicle.throttle) : 0) + " %"
                            color: "#cccccc"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

            }

            // ── 컨트롤 패널: ARM 토글 + 모드 버튼들 ────────────────
            // armed 상태에 따라 ARM/DISARM 텍스트와 색이 바뀌는 단일 버튼,
            // 그 아래로 MANUAL / STABILIZE / ALT_HOLD 모드 버튼.
            // 현재 모드와 일치하는 버튼은 active 강조색.
            Column {
                id: controlPanel
                visible: vehicle && vehicle.sysid > 0
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.topMargin: 8
                anchors.rightMargin: 8
                anchors.bottomMargin: 8
                width: 100
                spacing: 4

                // ARM/DISARM 토글
                Rectangle {
                    width: parent.width; height: 36
                    radius: 8
                    property bool isArmed: vehicle ? vehicle.armed : false
                    color: armBtnMa.containsMouse
                        ? (isArmed ? "#c93030" : "#3eaedb")
                        : (isArmed ? "#a82424" : "#61d3ff")
                    antialiasing: true

                    Text {
                        anchors.centerIn: parent
                        text: parent.isArmed ? "DISARM" : "ARM"
                        color: parent.isArmed ? "#ffffff" : "#0d1620"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.0
                    }

                    MouseArea {
                        id: armBtnMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (commander) commander.setArm(!parent.isArmed)
                    }
                }

                // 모드 버튼 빌더 — 동일 스타일 반복 회피
                Component {
                    id: modeBtnComponent
                    Rectangle {
                        property string modeName: ""
                        property string label: ""
                        property bool active: vehicle && vehicle.flightMode === modeName

                        width: controlPanel.width; height: 32
                        radius: 8
                        color: ma.containsMouse
                            ? (active ? "#3eaedb" : "#243341")
                            : (active ? "#61d3ff" : "#0d1620")
                        border.color: active ? "transparent" : "#2a3540"
                        border.width: 1
                        antialiasing: true

                        Text {
                            anchors.centerIn: parent
                            text: parent.label
                            color: parent.active ? "#0d1620" : "#cccccc"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.8
                        }
                        MouseArea {
                            id: ma
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (commander) commander.setMode(parent.modeName)
                        }
                    }
                }

                // 모드 버튼은 ARMED 상태에서만 노출 (DISARMED일 땐 ARM 버튼만 보임).
                // Column은 visible=false 자식을 레이아웃에서 제외하므로 공간도 함께 접힘.
                Loader { sourceComponent: modeBtnComponent;
                         visible: vehicle && vehicle.armed
                         onLoaded: { item.modeName = "MANUAL";    item.label = "MANUAL"; } }
                Loader { sourceComponent: modeBtnComponent;
                         visible: vehicle && vehicle.armed
                         onLoaded: { item.modeName = "STABILIZE"; item.label = "STABILIZE"; } }
                Loader { sourceComponent: modeBtnComponent;
                         visible: vehicle && vehicle.armed
                         onLoaded: { item.modeName = "ALT_HOLD";  item.label = "ALT HOLD"; } }
            }
        }
    }
}
