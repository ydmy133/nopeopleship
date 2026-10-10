import QtQuick 2.15
import QtQuick.Controls 2.15
import "../items"
import "../HudTheme.js" as Theme

Item {
    id: root
    signal requestClose()

    property int editingId: -1
    property Item editorItem: null

    function t(zh, en) {
        return Theme.tr(usv.settings.language, zh, en)
    }

    function commitName(id, text) {
        if (root.editingId !== id)
            return
        var n = String(text).trim()
        if (n.length > 0)
            usv.renameReturnPoint(id, n)
        root.editingId = -1
        root.editorItem = null
    }

    function cancelEdit() {
        root.editingId = -1
        root.editorItem = null
    }

    function clickInEditor(mx, my) {
        if (!root.editorItem || !root.editorItem.visible)
            return false
        var p = mapToItem(root.editorItem, mx, my)
        return p.x >= 0 && p.y >= 0 && p.x <= root.editorItem.width && p.y <= root.editorItem.height
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (root.editingId >= 0)
                root.cancelEdit()
            else
                root.requestClose()
        }
        onWheel: function(wheel) { wheel.accepted = true }
    }

    GlassPanel {
        width: 440
        height: Math.min(parent.height * 0.7, 580)
        anchors.left: parent.left
        anchors.leftMargin: 360
        anchors.top: parent.top
        anchors.topMargin: 168

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (root.editingId >= 0)
                    root.cancelEdit()
            }
            onWheel: function(wheel) { wheel.accepted = true }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12
            z: 1

            Text {
                text: t("返航", "Return")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }
            Text {
                text: t("单击已保存点开始返航。可保存当前船位为新返航点，定位图标可替换该点",
                        "Tap a saved point to return. Save current GPS, or pin to replace")
                color: Theme.textDim
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                width: parent.width
            }

            DarkButton {
                width: parent.width
                text: t("保存当前船位为返航点", "Save current GPS as return point")
                onClicked: {
                    if (root.editingId >= 0)
                        root.cancelEdit()
                    usv.saveReturnHere()
                }
            }

            DarkButton {
                width: parent.width
                visible: usv.control.oneKeyReturnActive
                text: t("停止返航", "Stop returning")
                onClicked: usv.control.toggleOneKeyReturn()
            }

            ListView {
                id: list
                width: parent.width
                height: Math.max(80, parent.height - y)
                clip: true
                model: usv.returnPoints
                spacing: 8
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                ScrollBar.vertical: ScrollBar { }

                Text {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    width: parent.width - 24
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    color: Theme.textMute
                    font.pixelSize: 13
                    text: t("还没有返航点，请先保存当前船位",
                            "No return points yet. Save the current GPS first")
                }

                delegate: Rectangle {
                    id: rowBox
                    width: list.width
                    height: 56
                    radius: 12
                    color: usv.control.navReturnId === pointId ? "#5530a0c8" : "#33101830"
                    border.color: usv.control.navReturnId === pointId ? Theme.cyan
                                  : (usv.settings.activeReturnId === pointId ? Theme.cyanMid : Theme.borderSoft)
                    border.width: 1
                    readonly property bool editing: root.editingId === pointId

                    Row {
                        id: row
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        Rectangle {
                            width: 28
                            height: 28
                            radius: 14
                            color: "#3318a0c8"
                            border.color: Theme.cyanMid
                            anchors.verticalCenter: parent.verticalCenter
                            HudIcon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                name: "pin"
                            }
                        }

                        Item {
                            width: parent.width - 28 - 10 - 76
                            height: parent.height
                            anchors.verticalCenter: parent.verticalCenter

                            Column {
                                anchors.fill: parent
                                visible: !rowBox.editing
                                Text {
                                    text: label + (usv.settings.activeReturnId === pointId
                                          ? t("  · 当前", "  · current") : "")
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.bold: true
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                Text {
                                    text: subtitle
                                    color: Theme.textDim
                                    font.pixelSize: 11
                                }
                            }

                            DarkTextField {
                                id: nameField
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                visible: rowBox.editing
                                enabled: rowBox.editing
                                z: 3
                                Keys.onPressed: function(event) {
                                    if (event.key === Qt.Key_Escape) {
                                        event.accepted = true
                                        root.cancelEdit()
                                    }
                                }
                                onVisibleChanged: {
                                    if (visible) {
                                        text = name
                                        root.editorItem = nameField
                                        Qt.callLater(function() {
                                            if (rowBox.editing) {
                                                nameField.forceActiveFocus()
                                                nameField.selectAll()
                                            }
                                        })
                                    } else if (root.editorItem === nameField) {
                                        root.editorItem = null
                                    }
                                }
                                onAccepted: root.commitName(pointId, text)
                            }
                        }

                        Item {
                            width: 28
                            height: 28
                            anchors.verticalCenter: parent.verticalCenter
                            HudIcon {
                                anchors.centerIn: parent
                                name: "pin"
                                width: 18
                                height: 18
                                stroke: Theme.cyan
                            }
                            MouseArea {
                                anchors.fill: parent
                                z: 2
                                onClicked: {
                                    if (rowBox.editing)
                                        root.cancelEdit()
                                    usv.updateReturnHere(pointId)
                                }
                            }
                        }
                        Item {
                            width: 28
                            height: 28
                            anchors.verticalCenter: parent.verticalCenter
                            HudIcon {
                                anchors.centerIn: parent
                                name: "close"
                                width: 14
                                height: 14
                                stroke: Theme.danger
                            }
                            MouseArea {
                                anchors.fill: parent
                                z: 2
                                onClicked: {
                                    if (rowBox.editing)
                                        root.cancelEdit()
                                    usv.removeReturnPoint(pointId)
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.rightMargin: 80
                        z: 1
                        enabled: !rowBox.editing
                        visible: !rowBox.editing
                        onClicked: {
                            if (root.editingId >= 0) {
                                root.cancelEdit()
                                return
                            }
                            clickTimer.restart()
                        }
                        onDoubleClicked: {
                            clickTimer.stop()
                            root.editingId = pointId
                        }
                    }
                    Timer {
                        id: clickTimer
                        interval: 220
                        onTriggered: usv.callReturnPoint(pointId)
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1000
        enabled: root.editingId >= 0
        visible: root.editingId >= 0
        hoverEnabled: false
        onPressed: {
            if (root.clickInEditor(mouse.x, mouse.y)) {
                mouse.accepted = false
                return
            }
            root.cancelEdit()
            mouse.accepted = true
        }
        onClicked: {
            mouse.accepted = true
        }
        onWheel: function(wheel) { wheel.accepted = true }
    }
}
