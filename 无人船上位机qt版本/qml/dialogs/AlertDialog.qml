import QtQuick 2.15
import "../items"
import "../HudTheme.js" as Theme

Item {
    id: root
    signal requestClose()

    function t(zh, en) {
        return Theme.tr(usv.settings.language, zh, en)
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.requestClose()
        onWheel: function(wheel) { wheel.accepted = true }
    }

    GlassPanel {
        width: 480
        height: 300
        anchors.right: parent.right
        anchors.rightMargin: parent.width * 0.16
        anchors.verticalCenter: parent.verticalCenter
        MouseArea {
            anchors.fill: parent
            onWheel: function(wheel) { wheel.accepted = true }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Text {
                text: t("风险警报", "Risk alerts")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }

            Repeater {
                model: [
                    { lv: t("高风险", "High"), c: Theme.danger, t: "#1 DCPA 最小会遇距离小于阈值", d: "RNG: 0.3NM  BRG: 23°" },
                    { lv: t("高风险", "High"), c: Theme.danger, t: "#2 DCPA 最小会遇距离小于阈值", d: "RNG: 0.3NM  BRG: 45°" },
                    { lv: t("中风险", "Medium"), c: Theme.warn, t: "#3 DCPA 最小会遇距离小于阈值", d: "RNG: 0.3NM  BRG: 23°" }
                ]
                Rectangle {
                    width: 430
                    height: 52
                    radius: 12
                    color: "#33101830"
                    border.color: "#33c4ecff"
                    Row {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10
                        Rectangle { width: 10; height: 10; radius: 5; color: modelData.c; anchors.verticalCenter: parent.verticalCenter }
                        Column {
                            Text { text: modelData.lv + "  " + modelData.t; color: Theme.text; font.pixelSize: 14; font.bold: true }
                            Text { text: modelData.d; color: Theme.textDim; font.pixelSize: 12 }
                        }
                    }
                }
            }
            Text {
                text: t("告警为占位示例（第一期无 AIS）", "Alerts are placeholders (no AIS in v1)")
                color: Theme.textMute
                font.pixelSize: 12
            }
        }
    }
}
