import QtQuick 2.15

Item {
    id: mapView
    clip: true

    Rectangle {
        anchors.fill: parent
        color: "#071828"
    }

    Item {
        id: tileLayer
        x: usv.map.layerX
        y: usv.map.layerY

        Repeater {
            model: usv.map
            delegate: Item {
                id: tile
                readonly property real sc: Math.pow(2, usv.map.zoom - model.tz)
                readonly property int nativeSize: usv.map.tileSize
                x: model.tx * nativeSize * sc - usv.map.originX
                y: model.ty * nativeSize * sc - usv.map.originY
                z: model.tz
                width: nativeSize * sc
                height: nativeSize * sc

                Image {
                    id: baseImg
                    anchors.fill: parent
                    source: model.imageUrl
                    asynchronous: true
                    cache: false
                    smooth: true
                    fillMode: Image.Stretch
                    sourceSize.width: tile.nativeSize
                    sourceSize.height: tile.nativeSize
                    property bool revealed: false
                    onSourceChanged: revealed = false
                    onStatusChanged: revealed = (status === Image.Ready)
                    visible: revealed
                }
                Image {
                    anchors.fill: parent
                    source: (model.overlayUrl.length > 0 && baseImg.revealed) ? model.overlayUrl : ""
                    asynchronous: true
                    cache: false
                    smooth: true
                    fillMode: Image.Stretch
                    sourceSize.width: tile.nativeSize
                    sourceSize.height: tile.nativeSize
                    property bool revealed: false
                    onSourceChanged: revealed = false
                    onStatusChanged: revealed = (status === Image.Ready)
                    visible: revealed
                }
            }
        }
    }

    BoatMarker {
        anchors.centerIn: parent
        visible: usv.map.boatVisible
        heading: usv.map.heading
        z: 40
    }

    onWidthChanged: usv.map.setViewport(Math.round(width), Math.round(height))
    onHeightChanged: usv.map.setViewport(Math.round(width), Math.round(height))
    Component.onCompleted: usv.map.setViewport(Math.round(width), Math.round(height))
}
