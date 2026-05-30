import QtQuick 2.12
import QtLocation 5.12
import QtPositioning 5.12

Item {
    anchors.fill: parent

    Map {
        id: _map
        anchors.fill: parent

        plugin: Plugin {
            name: "osm"
            // TileServer가 /gebco/ 경로를 GEBCO 타일 서버로 중계
            PluginParameter {
                name: "osm.mapping.custom.host"
                value: "http://127.0.0.1:17777/gebco/"
            }
            PluginParameter {
                name: "osm.mapping.cache.directory"
                value: bridge ? bridge.tileCachePath + "/gebco" : "/tmp/holyuuv_tiles/gebco"
            }
            PluginParameter { name: "osm.mapping.cache.disk.cost_strategy"; value: "unitary" }
        }

        center: QtPositioning.coordinate(37.52951029463262, 126.94149832867085)
        zoomLevel: 6
        minimumZoomLevel: 0
        maximumZoomLevel: 10.9  // ESRI Ocean 데이터 최대 줌레벨

        onCenterChanged: {
            if (bridge) bridge.updateMapCenter(center.latitude, center.longitude)
        }

        Component.onCompleted: {
            for (var i = 0; i < supportedMapTypes.length; i++) {
                if (supportedMapTypes[i].style === MapType.CustomMap) {
                    activeMapType = supportedMapTypes[i]
                    break
                }
            }
        }

        MapQuickItem {
            id: _vehicleMarker
            visible: bridge ? bridge.hasPosition : false
            coordinate: QtPositioning.coordinate(
                bridge ? bridge.latitude  : 0,
                bridge ? bridge.longitude : 0)
            anchorPoint.x: _dot.width  / 2
            anchorPoint.y: _dot.height / 2

            sourceItem: Rectangle {
                id: _dot
                width: 16; height: 16
                color: "#00e5ff"
                radius: 8
                border.color: "white"
                border.width: 2
            }
        }

        Connections {
            target: bridge
            function onPositionChanged() {
                _vehicleMarker.coordinate = QtPositioning.coordinate(
                    bridge.latitude, bridge.longitude)
            }
        }

        // 줌레벨 표시
        Text {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 8
            text: "zoom: " + _map.zoomLevel.toFixed(1)
            color: "white"
            font.pixelSize: 14
            style: Text.Outline
            styleColor: "black"
        }
    }
}
