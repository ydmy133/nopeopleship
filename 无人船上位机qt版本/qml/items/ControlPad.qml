import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: pad
    width: 500
    height: 500

    Canvas {
        id: rings
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
            var R = Math.min(w, h)

            var fill = ctx.createRadialGradient(cx, cy, R * 0.08, cx, cy, R * 0.48)
            fill.addColorStop(0, "rgba(30, 90, 130, 0.10)")
            fill.addColorStop(1, "rgba(30, 90, 130, 0.00)")
            ctx.fillStyle = fill
            ctx.beginPath()
            ctx.arc(cx, cy, R * 0.48, 0, Math.PI * 2)
            ctx.fill()

            var radii = [0.47, 0.365, 0.255, 0.12]
            for (var i = 0; i < radii.length; i++) {
                ctx.beginPath()
                ctx.arc(cx, cy, R * radii[i], 0, Math.PI * 2)
                ctx.strokeStyle = i === 0 ? "rgba(150, 220, 240, 0.22)" : "rgba(150, 220, 240, 0.28)"
                ctx.lineWidth = i === 1 ? 1.6 : 1.15
                ctx.stroke()
            }

            for (var t = 0; t < 72; t++) {
                var ang = (t * 5 - 90) * Math.PI / 180
                var major = (t % 6 === 0)
                var r0 = R * 0.47
                var r1 = R * (major ? 0.445 : 0.458)
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(ang) * r0, cy + Math.sin(ang) * r0)
                ctx.lineTo(cx + Math.cos(ang) * r1, cy + Math.sin(ang) * r1)
                ctx.strokeStyle = major ? "rgba(190, 236, 250, 0.35)" : "rgba(170, 220, 240, 0.16)"
                ctx.lineWidth = major ? 1.2 : 0.8
                ctx.stroke()
            }

            ctx.strokeStyle = "rgba(180, 230, 245, 0.18)"
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.moveTo(cx, cy - R * 0.08)
            ctx.lineTo(cx, cy + R * 0.08)
            ctx.moveTo(cx - R * 0.08, cy)
            ctx.lineTo(cx + R * 0.08, cy)
            ctx.stroke()

            ctx.beginPath()
            ctx.arc(cx, cy, 3.2, 0, Math.PI * 2)
            ctx.fillStyle = "rgba(210, 245, 255, 0.55)"
            ctx.fill()
        }
    }

    PadArrow {
        action: "forward"
        anchors.horizontalCenter: parent.horizontalCenter
        y: 58
    }
    PadArrow {
        action: "backward"
        rotation: 180
        anchors.horizontalCenter: parent.horizontalCenter
        y: 286
    }
    PadArrow {
        action: "turnLeft"
        rotation: 270
        x: 52
        y: 188
    }
    PadArrow {
        action: "turnRight"
        rotation: 90
        x: 352
        y: 188
    }
}
