import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: hud
    width: 900
    height: 128

    function t(zh, en) {
        return Theme.tr(usv.settings.language, zh, en)
    }

    function fmtMs(v) {
        return (typeof v === "number" && isFinite(v)) ? v.toFixed(1) : "--"
    }
    function depthFill() {
        var v = usv.telemetry.altitude
        if (typeof v !== "number" || !isFinite(v))
            return 0.42
        var t = (v - (-1.5)) / (0.8 - (-1.5))
        return Math.max(0.08, Math.min(0.92, t))
    }

    GlassSurface {
        anchors.fill: parent
        radius: 18
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 24
        anchors.topMargin: 14
        anchors.bottomMargin: 14
        spacing: 18

        Item {
            width: 50
            height: parent.height
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                text: usv.telemetry.depthTopLabel
                color: Theme.textMute
                font.family: Theme.fontNum
                font.pixelSize: 11
            }
            Rectangle {
                width: 8
                height: parent.height - 32
                radius: 4
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: "#33101830"
                border.color: "#66A8D4E8"
                border.width: 1
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: parent.height * hud.depthFill()
                    radius: 4
                    color: Theme.cyanMid
                    opacity: 0.85
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                text: usv.telemetry.depthBottomLabel
                color: Theme.textMute
                font.family: Theme.fontNum
                font.pixelSize: 11
            }
        }

        Rectangle { width: 1; height: parent.height * 0.78; anchors.verticalCenter: parent.verticalCenter; color: "#33C4E8FF" }

        Item {
            width: 220
            height: parent.height
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Row {
                    spacing: 8
                    Text {
                        text: hud.fmtMs(usv.telemetry.speedMs)
                        color: Theme.text
                        font.family: Theme.fontNum
                        font.pixelSize: 40
                        font.weight: Font.Light
                    }
                    Text {
                        text: "m/s"
                        color: Theme.textDim
                        font.family: Theme.fontNum
                        font.pixelSize: 14
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                    }
                }
                Row {
                    spacing: 6
                    HudIcon { name: "wave"; width: 18; height: 14; stroke: Theme.cyanMid; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: t("当前速度", "Speed")
                        color: Theme.textMute
                        font.family: Theme.fontUi
                        font.pixelSize: 12
                    }
                }
                Text {
                    text: usv.telemetry.latDisplay + " " + usv.telemetry.latDir
                          + "   " + usv.telemetry.lonDisplay + " " + usv.telemetry.lonDir
                    color: Theme.textMute
                    font.family: Theme.fontNum
                    font.pixelSize: 10
                    opacity: 0.7
                }
            }
        }

        Rectangle { width: 1; height: parent.height * 0.78; anchors.verticalCenter: parent.verticalCenter; color: "#33C4E8FF" }

        Item {
            width: 108
            height: parent.height
            Column {
                anchors.centerIn: parent
                spacing: 0
                HudIcon {
                    name: "north"
                    width: 16
                    height: 22
                    anchors.horizontalCenter: parent.horizontalCenter
                    stroke: Theme.cyan
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "N"
                    color: Theme.text
                    font.family: Theme.fontNum
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: t("航向", "HDG")
                    color: Theme.textMute
                    font.family: Theme.fontUi
                    font.pixelSize: 11
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: usv.telemetry.hdgDisplay
                    color: Theme.text
                    font.family: Theme.fontNum
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle { width: 1; height: parent.height * 0.78; anchors.verticalCenter: parent.verticalCenter; color: "#33C4E8FF" }

        Row {
            spacing: 28
            anchors.verticalCenter: parent.verticalCenter
            Column {
                spacing: 4
                Text { text: "SOG"; color: Theme.textMute; font.family: Theme.fontUi; font.pixelSize: 11 }
                Text {
                    text: hud.fmtMs(usv.telemetry.speedMs)
                    color: Theme.text
                    font.family: Theme.fontNum
                    font.pixelSize: 24
                    font.weight: Font.Light
                }
                Text { text: "m/s"; color: Theme.textMute; font.family: Theme.fontNum; font.pixelSize: 11 }
            }
            Column {
                spacing: 4
                Text { text: "COG"; color: Theme.textMute; font.family: Theme.fontUi; font.pixelSize: 11 }
                Text {
                    text: usv.telemetry.cogDisplay
                    color: Theme.text
                    font.family: Theme.fontNum
                    font.pixelSize: 24
                    font.weight: Font.Light
                }
            }
            Column {
                spacing: 4
                Text { text: "HDG"; color: Theme.textMute; font.family: Theme.fontUi; font.pixelSize: 11 }
                Text {
                    text: usv.telemetry.hdgDisplay
                    color: Theme.text
                    font.family: Theme.fontNum
                    font.pixelSize: 24
                    font.weight: Font.Light
                }
            }
        }
    }
}
