import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property string icon: "pin"
    property bool active: false
    property bool danger: false
    property bool showChevron: true
    signal clicked()

    width: 300
    height: 70

    GlassSurface {
        anchors.fill: parent
        pill: true
        danger: root.danger
        active: root.active
        hovered: mouse.containsMouse
    }

    Canvas {
        visible: root.danger
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 16
        height: parent.height - 18
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = Theme.danger
            ctx.lineWidth = 2.2
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width + 8, height / 2, height * 0.40, Math.PI * 0.72, Math.PI * 1.28)
            ctx.stroke()
        }
        Component.onCompleted: requestPaint()
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 12

        Rectangle {
            width: 42
            height: 42
            radius: 21
            anchors.verticalCenter: parent.verticalCenter
            color: root.danger ? "#33A05048" : (root.active ? "#4428A0C0" : "#2A1C6888")
            border.color: root.danger ? Theme.danger : Theme.cyanMid
            border.width: 1
            HudIcon {
                anchors.centerIn: parent
                width: 22
                height: 22
                name: root.icon
                stroke: root.danger ? Theme.danger : Theme.cyan
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 42 - 12 - (root.showChevron ? 18 : 0) - 8
            spacing: 3
            Text {
                text: root.title
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                width: parent.width
            }
            Text {
                text: root.subtitle
                color: Theme.textMute
                font.family: Theme.fontUi
                font.pixelSize: 11
                elide: Text.ElideRight
                width: parent.width
            }
        }
    }

    HudIcon {
        visible: root.showChevron
        name: "chevron"
        width: 14
        height: 24
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        stroke: Theme.textMute
        opacity: 0.75
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        onWheel: function(wheel) { wheel.accepted = true }
    }
}
