import QtQuick 2.15
import QtQuick.Controls 2.15
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
        height: 400
        anchors.right: parent.right
        anchors.rightMargin: parent.width * 0.18
        anchors.verticalCenter: parent.verticalCenter
        MouseArea {
            anchors.fill: parent
            onWheel: function(wheel) { wheel.accepted = true }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            Text {
                text: t("速度模式", "Speed mode")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }

            Row {
                spacing: 16
                Text {
                    text: t("定速巡航：", "Speed cruise:")
                    color: Theme.text
                    font.pixelSize: 16
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Switch {
                    checked: usv.control.cruiseSpeedOn
                    onToggled: {
                        if (checked !== usv.control.cruiseSpeedOn)
                            usv.control.toggleCruiseSpeed()
                    }
                }
            }
            Row {
                spacing: 16
                Text {
                    text: t("定向巡航：", "Heading cruise:")
                    color: Theme.text
                    font.pixelSize: 16
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Switch {
                    checked: usv.control.headingCruiseOn
                    onToggled: {
                        if (checked !== usv.control.headingCruiseOn)
                            usv.control.toggleHeadingCruise()
                    }
                }
            }
            Row {
                spacing: 16
                Text {
                    text: t("航迹保持：", "Track hold:")
                    color: Theme.text
                    font.pixelSize: 16
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Switch {
                    checked: usv.control.trackHoldOn
                    onToggled: {
                        if (checked !== usv.control.trackHoldOn)
                            usv.control.toggleTrackHold()
                    }
                }
            }

            SpeedRow {
                width: parent.width
                label: t("行驶速度：", "Drive speed:")
                value: usv.settings.moveSpeedPercent
                onMoved: usv.settings.moveSpeedPercent = value
            }
            SpeedRow {
                width: parent.width
                label: t("转向速度：", "Steer speed:")
                value: usv.settings.steerSpeedPercent
                onMoved: usv.settings.steerSpeedPercent = value
            }
        }
    }

    component SpeedRow: Row {
        id: row
        property string label: ""
        property int value: 20
        signal moved(int value)

        spacing: 10
        height: Math.max(labelText.height, slider.height)

        Text {
            id: labelText
            width: 92
            text: row.label
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
        Slider {
            id: slider
            width: parent.width - labelText.width - pctText.width - parent.spacing * 2
            from: 0
            to: 100
            stepSize: 1
            live: true
            value: row.value
            onMoved: row.moved(Math.round(value))
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: pctText
            width: 48
            text: Math.round(slider.value) + "%"
            color: Theme.cyan
            font.pixelSize: 16
            font.bold: true
            horizontalAlignment: Text.AlignRight
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
