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
            PluginParameter {
                name: "osm.mapping.custom.host"
                value: "https://a.basemaps.cartocdn.com/dark_all/"
            }
            PluginParameter {
                name: "osm.mapping.cache.directory"
                value: bridge ? bridge.tileCachePath : "/tmp/holyuuv_tiles"
            }
            PluginParameter { name: "osm.mapping.cache.disk.cost_strategy"; value: "unitary" }
        }

        center: QtPositioning.coordinate(35.074857, 129.084836)
        zoomLevel: 10

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
    }
}
