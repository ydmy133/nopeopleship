import QtQuick 2.15
import QtQuick.Controls 2.15
import "../HudTheme.js" as Theme

Button {
    id: control
    font.pixelSize: 15
    leftPadding: 12
    rightPadding: 12
    topPadding: 6
    bottomPadding: 6

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? Theme.text : Theme.textMute
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 96
        implicitHeight: 32
        radius: 6
        color: control.down ? "#050505" : (control.hovered ? "#1a1a1a" : "#0a0a0a")
        border.color: control.down ? Theme.cyan : Theme.borderSoft
        border.width: 1
    }
}
