import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: rose
    property real heading: 0
    width: 216
    height: 216

    Canvas {
        id: ticks
        anchors.fill: parent
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)
            var cx = w / 2
            var cy = h / 2
            var r = Math.min(w, h) * 0.40

            var glow = ctx.createRadialGradient(cx, cy, r * 0.15, cx, cy, r + 10)
            glow.addColorStop(0, "rgba(18, 70, 108, 0.16)")
            glow.addColorStop(1, "rgba(18, 70, 110, 0.00)")
            ctx.fillStyle = glow
            ctx.beginPath()
            ctx.arc(cx, cy, r + 8, 0, Math.PI * 2)
            ctx.fill()

            ctx.strokeStyle = "rgba(140, 210, 235, 0.12)"
            ctx.lineWidth = 1
            for (var g = 1; g <= 3; g++) {
                ctx.beginPath()
                ctx.arc(cx, cy, r * (0.32 * g), 0, Math.PI * 2)
                ctx.stroke()
            }
            for (var k = 0; k < 8; k++) {
                var ga = (k * 45 - 90) * Math.PI / 180
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(ga) * r * 0.18, cy + Math.sin(ga) * r * 0.18)
                ctx.lineTo(cx + Math.cos(ga) * r * 0.92, cy + Math.sin(ga) * r * 0.92)
                ctx.stroke()
            }

            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.strokeStyle = "rgba(110, 200, 230, 0.22)"
            ctx.lineWidth = 8
            ctx.stroke()

            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.strokeStyle = Theme.cyan
            ctx.lineWidth = 2.2
            ctx.stroke()

            for (var i = 0; i < 72; i++) {
                var a = (i * 5 - 90) * Math.PI / 180
                var major = (i % 6 === 0)
                var r0 = r - (major ? 11 : 6)
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r)
                ctx.lineTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0)
                ctx.strokeStyle = major ? "rgba(210, 246, 255, 0.85)" : "rgba(160, 220, 240, 0.32)"
                ctx.lineWidth = major ? 1.6 : 0.9
                ctx.stroke()
            }
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        text: "0°"
        color: Theme.text
        font.family: Theme.fontNum
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        text: "90°"
        color: Theme.text
        font.family: Theme.fontNum
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        text: "180°"
        color: Theme.text
        font.family: Theme.fontNum
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        text: "270°"
        color: Theme.text
        font.family: Theme.fontNum
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    Canvas {
        id: needle
        anchors.centerIn: parent
        width: parent.width * 0.38
        height: parent.height * 0.38
        rotation: rose.heading
        antialiasing: true
        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)
            ctx.translate(w / 2, h / 2)
            ctx.beginPath()
            ctx.moveTo(0, -h * 0.44)
            ctx.lineTo(w * 0.14, h * 0.08)
            ctx.lineTo(0, -h * 0.04)
            ctx.closePath()
            ctx.fillStyle = "#CFF6FF"
            ctx.fill()
            ctx.beginPath()
            ctx.moveTo(0, -h * 0.44)
            ctx.lineTo(-w * 0.14, h * 0.08)
            ctx.lineTo(0, -h * 0.04)
            ctx.closePath()
            ctx.fillStyle = Theme.cyan
            ctx.fill()
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(w * 0.09, h * 0.30)
            ctx.lineTo(0, h * 0.20)
            ctx.lineTo(-w * 0.09, h * 0.30)
            ctx.closePath()
            ctx.fillStyle = "rgba(170, 210, 225, 0.40)"
            ctx.fill()
        }
        Component.onCompleted: requestPaint()
    }
}
