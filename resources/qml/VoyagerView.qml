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
            // TileServer(127.0.0.1:17777)가 SQLite 캐시 확인 후 Voyager로 프록시
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

        center: QtPositioning.coordinate(37.52951029463262, 126.94149832867085)
        zoomLevel: 15

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

        Text {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 8
            text: "zoom: " + _map.zoomLevel.toFixed(1)
            color: "black"
            font.pixelSize: 14
            style: Text.Outline
            styleColor: "white"
        }
    }
}
