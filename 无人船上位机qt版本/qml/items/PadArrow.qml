import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: arrow
    property string action: "forward"
    property bool lit: usv.control.pulsingKey === action
    width: 96
    height: 112
    scale: mouse.pressed || lit ? 1.06 : (mouse.containsMouse ? 1.03 : 1)
    Behavior on scale { NumberAnimation { duration: 80 } }

    Canvas {
        anchors.fill: parent
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)

            var glow = ctx.createRadialGradient(w / 2, h * 0.48, 6, w / 2, h * 0.5, w * 0.62)
            glow.addColorStop(0, "rgba(170, 235, 255, 0.22)")
            glow.addColorStop(1, "rgba(170, 235, 255, 0)")
            ctx.fillStyle = glow
            ctx.fillRect(0, 0, w, h)

            ctx.globalAlpha = 0.92
            ctx.beginPath()
            ctx.moveTo(w * 0.50, h * 0.10)
            ctx.lineTo(w * 0.90, h * 0.84)
            ctx.lineTo(w * 0.50, h * 0.58)
            ctx.closePath()
            var right = ctx.createLinearGradient(w * 0.50, h * 0.10, w * 0.90, h * 0.84)
            right.addColorStop(0, "#F7FFFF")
            right.addColorStop(0.4, "#C8F0FA")
            right.addColorStop(1, "#5AA8C8")
            ctx.fillStyle = right
            ctx.fill()

            ctx.beginPath()
            ctx.moveTo(w * 0.50, h * 0.10)
            ctx.lineTo(w * 0.50, h * 0.58)
            ctx.lineTo(w * 0.10, h * 0.84)
            ctx.closePath()
            var left = ctx.createLinearGradient(w * 0.12, h * 0.84, w * 0.50, h * 0.10)
            left.addColorStop(0, "#2E7A9C")
            left.addColorStop(1, "#B5E6F4")
            ctx.fillStyle = left
            ctx.fill()

            ctx.globalAlpha = 1
            ctx.beginPath()
            ctx.moveTo(w * 0.50, h * 0.10)
            ctx.lineTo(w * 0.90, h * 0.84)
            ctx.lineTo(w * 0.10, h * 0.84)
            ctx.closePath()
            ctx.strokeStyle = "rgba(240, 252, 255, 0.75)"
            ctx.lineWidth = 1.3
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(w * 0.50, h * 0.18)
            ctx.lineTo(w * 0.50, h * 0.52)
            ctx.strokeStyle = "rgba(255, 255, 255, 0.35)"
            ctx.lineWidth = 1
            ctx.stroke()
        }
        Component.onCompleted: requestPaint()
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: usv.control.controlDown(arrow.action)
        onReleased: usv.control.controlUp(arrow.action)
        onCanceled: usv.control.controlUp(arrow.action)
    }
}
