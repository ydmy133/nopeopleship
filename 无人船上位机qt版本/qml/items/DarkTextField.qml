import QtQuick 2.15
import QtQuick.Controls 2.15
import "../HudTheme.js" as Theme

TextField {
    id: control
    color: Theme.text
    placeholderTextColor: Theme.textMute
    selectedTextColor: "#ffffff"
    selectionColor: "#2aa8d0"
    font.pixelSize: 15
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6
    selectByMouse: true

    background: Rectangle {
        implicitWidth: 320
        implicitHeight: 32
        radius: 6
        color: "#0a0a0a"
        border.color: control.activeFocus ? Theme.cyan : Theme.borderSoft
        border.width: 1
    }
}
