import QtQuick 2.15
import "HudTheme.js" as Theme

Rectangle {
    id: toast
    property int lastNonce: 0
    width: Math.min(parent.width * 0.6, Math.max(280, msg.implicitWidth + 40))
    height: 44
    radius: 22
    color: usv.lastToastLevel === "warning" ? "#cc8a4a12"
         : usv.lastToastLevel === "error" ? "#cc7a2428"
         : "#cc1c4a68"
    border.color: usv.lastToastLevel === "warning" ? Theme.warn
                : usv.lastToastLevel === "error" ? Theme.danger
                : Theme.cyan
    border.width: 1
    opacity: 0
    visible: opacity > 0.05

    Text {
        id: msg
        anchors.centerIn: parent
        color: Theme.text
        font.pixelSize: 16
        font.bold: true
        text: usv.lastToast
    }

    Connections {
        target: usv
        function onToastChanged() {
            if (usv.toastNonce === toast.lastNonce)
                return
            toast.lastNonce = usv.toastNonce
            showAnim.restart()
        }
    }

    SequentialAnimation {
        id: showAnim
        NumberAnimation { target: toast; property: "opacity"; to: 0.92; duration: 120 }
        PauseAnimation { duration: 2600 }
        NumberAnimation { target: toast; property: "opacity"; to: 0; duration: 280 }
    }
}
