import QtQuick 2.15
import QtQuick.Controls 2.15
import "../items"
import "../HudTheme.js" as Theme

Item {
    id: root
    signal requestClose()

    property int editingNumber: -1
    property Item editorItem: null

    function t(zh, en) {
        return Theme.tr(usv.settings.language, zh, en)
    }

    function commitName(number, text) {
        if (root.editingNumber !== number)
            return
        var n = String(text).trim()
        if (n.length > 0)
            usv.renamePreset(number, n)
        root.editingNumber = -1
        root.editorItem = null
    }

    function cancelEdit() {
        root.editingNumber = -1
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
            if (root.editingNumber >= 0)
                root.cancelEdit()
            else
                root.requestClose()
        }
        onWheel: function(wheel) { wheel.accepted = true }
    }

    GlassPanel {
        width: 440
        height: Math.min(parent.height * 0.7, 560)
        anchors.right: parent.right
        anchors.rightMargin: parent.width * 0.18
        anchors.verticalCenter: parent.verticalCenter

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (root.editingNumber >= 0)
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
                text: t("点到点导航", "Point to point")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }
            Text {
                text: t("单击前往，双击改名。定位图标保存或替换为当前船位，清除可删除该点",
                        "Tap to go, double-tap to rename. Pin saves/replaces, X clears")
                color: Theme.textDim
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                width: parent.width
            }

            ListView {
                id: list
                width: parent.width
                height: parent.height - 92
                clip: true
                model: usv.presets
                spacing: 8
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                ScrollBar.vertical: ScrollBar { }
                delegate: Rectangle {
                    id: rowBox
                    width: list.width
                    height: 56
                    radius: 12
                    color: usv.control.navPresetNumber === positionNumber ? "#5530a0c8" : "#33101830"
                    border.color: usv.control.navPresetNumber === positionNumber ? Theme.cyan : Theme.borderSoft
                    border.width: 1
                    readonly property bool editing: root.editingNumber === positionNumber

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
                                name: "nav"
                            }
                        }

                        Item {
                            width: parent.width - 28 - 10 - (hasCoord ? 76 : 38)
                            height: parent.height
                            anchors.verticalCenter: parent.verticalCenter

                            Column {
                                anchors.fill: parent
                                visible: !rowBox.editing
                                Text {
                                    text: label
                                    color: Theme.text
                                    font.pixelSize: 15
                                    font.bold: true
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                Text {
                                    text: hasCoord ? subtitle : t("未设置坐标", "No coordinates")
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
                                onAccepted: root.commitName(positionNumber, text)
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
                                stroke: hasCoord ? Theme.cyan : Theme.textDim
                            }
                            MouseArea {
                                anchors.fill: parent
                                z: 2
                                onClicked: {
                                    if (rowBox.editing)
                                        root.cancelEdit()
                                    usv.savePresetHere(positionNumber)
                                }
                            }
                        }
                        Item {
                            width: 28
                            height: 28
                            anchors.verticalCenter: parent.verticalCenter
                            visible: hasCoord
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
                                    usv.clearPreset(positionNumber)
                                }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.rightMargin: hasCoord ? 80 : 48
                        z: 1
                        enabled: !rowBox.editing
                        visible: !rowBox.editing
                        onClicked: {
                            if (root.editingNumber >= 0) {
                                root.cancelEdit()
                                return
                            }
                            clickTimer.restart()
                        }
                        onDoubleClicked: {
                            clickTimer.stop()
                            root.editingNumber = positionNumber
                        }
                    }
                    Timer {
                        id: clickTimer
                        interval: 220
                        onTriggered: usv.callPreset(positionNumber)
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1000
        enabled: root.editingNumber >= 0
        visible: root.editingNumber >= 0
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
