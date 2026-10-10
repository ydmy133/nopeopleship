import QtQuick 2.15
import "HudTheme.js" as Theme

Item {
    id: splash
    visible: true

    function t(zh, en) {
        return Theme.tr(usv.settings.language, zh, en)
    }

    Image {
        anchors.fill: parent
        source: "qrc:/equ/background.png"
        fillMode: Image.PreserveAspectCrop
    }
    Rectangle {
        anchors.fill: parent
        color: "#99081420"
    }

    Column {
        anchors.centerIn: parent
        spacing: 20

        Rectangle {
            width: 64
            height: 64
            radius: 16
            color: "#5530a0c8"
            border.color: Theme.cyan
            border.width: 1
            anchors.horizontalCenter: parent.horizontalCenter
            Canvas {
                anchors.centerIn: parent
                width: 32
                height: 32
                antialiasing: true
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = Theme.cyan
                    ctx.beginPath()
                    ctx.moveTo(width / 2, 4)
                    ctx.lineTo(width * 0.86, height * 0.86)
                    ctx.lineTo(width / 2, height * 0.64)
                    ctx.lineTo(width * 0.14, height * 0.86)
                    ctx.closePath()
                    ctx.fill()
                }
                Component.onCompleted: requestPaint()
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: t("无人船控制系统", "USV Control System")
            color: Theme.text
            font.pixelSize: 36
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: usv.linkStatus
            color: Theme.textDim
            font.pixelSize: 16
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 280
            height: 6
            radius: 3
            color: "#33204058"
            Rectangle {
                id: bar
                width: parent.width * 0.2
                height: parent.height
                radius: 3
                color: Theme.cyan
                SequentialAnimation on width {
                    loops: Animation.Infinite
                    NumberAnimation { to: 280; duration: 900 }
                    NumberAnimation { to: 60; duration: 500 }
                }
            }
        }
    }
}
