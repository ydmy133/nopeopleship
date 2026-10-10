import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: marker
    property real heading: 0
    width: 56
    height: 56

    Canvas {
        id: needle
        anchors.fill: parent
        antialiasing: true
        renderTarget: Canvas.FramebufferObject
        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)
            ctx.save()
            ctx.translate(w / 2, h / 2)
            ctx.beginPath()
            ctx.moveTo(0, -h * 0.42)
            ctx.lineTo(w * 0.18, h * 0.28)
            ctx.lineTo(0, h * 0.12)
            ctx.lineTo(-w * 0.18, h * 0.28)
            ctx.closePath()
            ctx.fillStyle = Theme.cyan
            ctx.strokeStyle = "#0A2848"
            ctx.lineWidth = 2
            ctx.fill()
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(0, 0, w * 0.08, 0, Math.PI * 2)
            ctx.fillStyle = "#F4FBFF"
            ctx.fill()
            ctx.restore()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    transform: Rotation {
        origin.x: marker.width / 2
        origin.y: marker.height / 2
        angle: marker.heading
    }
}
