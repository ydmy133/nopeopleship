import QtQuick 2.15
import "../HudTheme.js" as Theme

Canvas {
    id: icon
    property string name: "pin"
    property color stroke: Theme.cyan
    property real line: Math.max(1.15, Math.min(width, height) * 0.07)
    antialiasing: true
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onNameChanged: requestPaint()
    onStrokeChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        var w = width
        var h = height
        ctx.clearRect(0, 0, w, h)
        ctx.strokeStyle = icon.stroke
        ctx.fillStyle = icon.stroke
        ctx.lineWidth = icon.line
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        var cx = w / 2
        var cy = h / 2
        var s = Math.min(w, h)

        function circle(x, y, r, fill) {
            ctx.beginPath()
            ctx.arc(x, y, r, 0, Math.PI * 2)
            if (fill)
                ctx.fill()
            else
                ctx.stroke()
        }

        if (name === "pin") {
            ctx.beginPath()
            ctx.arc(cx, h * 0.38, s * 0.22, Math.PI * 0.9, Math.PI * 2.1)
            ctx.lineTo(cx, h * 0.92)
            ctx.closePath()
            ctx.stroke()
            circle(cx, h * 0.36, s * 0.09, false)
        } else if (name === "stop") {
            var r = s * 0.16
            ctx.beginPath()
            ctx.moveTo(cx - s * 0.18, cy - s * 0.18)
            ctx.lineTo(cx + s * 0.18, cy - s * 0.18)
            ctx.lineTo(cx + s * 0.18, cy + s * 0.18)
            ctx.lineTo(cx - s * 0.18, cy + s * 0.18)
            ctx.closePath()
            ctx.fill()
        } else if (name === "nav") {
            ctx.beginPath()
            ctx.arc(cx, h * 0.40, s * 0.20, Math.PI * 0.85, Math.PI * 2.15)
            ctx.lineTo(cx, h * 0.90)
            ctx.closePath()
            ctx.stroke()
            circle(cx, h * 0.38, s * 0.08, true)
        } else if (name === "speed") {
            circle(cx, cy, s * 0.32, false)
            ctx.beginPath()
            ctx.moveTo(cx, cy)
            ctx.lineTo(cx + s * 0.18, cy - s * 0.16)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(cx, cy, s * 0.32, Math.PI * 0.75, Math.PI * 0.25)
            ctx.stroke()
        } else if (name === "anchor") {
            ctx.beginPath()
            ctx.arc(cx, h * 0.28, s * 0.10, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx, h * 0.38)
            ctx.lineTo(cx, h * 0.78)
            ctx.moveTo(cx - s * 0.18, h * 0.48)
            ctx.lineTo(cx + s * 0.18, h * 0.48)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(cx, h * 0.62, s * 0.22, Math.PI * 0.15, Math.PI * 0.85)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(cx, h * 0.62, s * 0.22, Math.PI * 1.15, Math.PI * 1.85)
            ctx.stroke()
        } else if (name === "alert") {
            ctx.beginPath()
            ctx.moveTo(cx, h * 0.16)
            ctx.lineTo(w * 0.84, h * 0.84)
            ctx.lineTo(cx - s * 0.34, h * 0.84)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx, h * 0.40)
            ctx.lineTo(cx, h * 0.60)
            ctx.stroke()
            circle(cx, h * 0.72, s * 0.035, true)
        } else if (name === "gear") {
            circle(cx, cy, s * 0.14, false)
            for (var i = 0; i < 8; i++) {
                var a = i * Math.PI / 4
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(a) * s * 0.22, cy + Math.sin(a) * s * 0.22)
                ctx.lineTo(cx + Math.cos(a) * s * 0.34, cy + Math.sin(a) * s * 0.34)
                ctx.stroke()
            }
        } else if (name === "chevron") {
            ctx.beginPath()
            ctx.moveTo(w * 0.38, h * 0.28)
            ctx.lineTo(w * 0.62, h * 0.50)
            ctx.lineTo(w * 0.38, h * 0.72)
            ctx.stroke()
        } else if (name === "back") {
            ctx.beginPath()
            ctx.moveTo(w * 0.62, h * 0.28)
            ctx.lineTo(w * 0.38, h * 0.50)
            ctx.lineTo(w * 0.62, h * 0.72)
            ctx.stroke()
        } else if (name === "wave") {
            ctx.beginPath()
            ctx.moveTo(w * 0.12, cy)
            ctx.quadraticCurveTo(w * 0.28, cy - s * 0.18, w * 0.42, cy)
            ctx.quadraticCurveTo(w * 0.56, cy + s * 0.18, w * 0.70, cy)
            ctx.quadraticCurveTo(w * 0.82, cy - s * 0.12, w * 0.90, cy)
            ctx.stroke()
        } else if (name === "north") {
            ctx.beginPath()
            ctx.moveTo(cx, h * 0.12)
            ctx.lineTo(cx + s * 0.16, cy)
            ctx.lineTo(cx, h * 0.42)
            ctx.lineTo(cx - s * 0.16, cy)
            ctx.closePath()
            ctx.fill()
            ctx.beginPath()
            ctx.moveTo(cx, h * 0.42)
            ctx.lineTo(cx, h * 0.88)
            ctx.stroke()
        } else if (name === "wifi") {
            for (var k = 1; k <= 3; k++) {
                ctx.beginPath()
                ctx.arc(cx, h * 0.72, s * 0.12 * k, Math.PI * 1.18, Math.PI * 1.82)
                ctx.stroke()
            }
            circle(cx, h * 0.80, s * 0.04, true)
        } else if (name === "logo") {
            ctx.beginPath()
            ctx.moveTo(cx, h * 0.12)
            ctx.lineTo(w * 0.86, h * 0.86)
            ctx.lineTo(cx, h * 0.64)
            ctx.lineTo(w * 0.14, h * 0.86)
            ctx.closePath()
            ctx.fill()
        } else if (name === "close") {
            ctx.beginPath()
            ctx.moveTo(cx - s * 0.22, cy - s * 0.22)
            ctx.lineTo(cx + s * 0.22, cy + s * 0.22)
            ctx.moveTo(cx + s * 0.22, cy - s * 0.22)
            ctx.lineTo(cx - s * 0.22, cy + s * 0.22)
            ctx.stroke()
        } else if (name === "satellite") {
            circle(cx, cy, s * 0.16, false)
            ctx.beginPath()
            ctx.arc(cx, cy, s * 0.30, 0.2, 1.2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(cx, cy, s * 0.30, 3.4, 4.2)
            ctx.stroke()
        }
    }
}
