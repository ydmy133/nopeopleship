import QtQuick 2.15
import "../HudTheme.js" as Theme

Canvas {
    id: bar
    property real angle: 0
    property real current: 0
    height: 92

    Timer {
        interval: 16
        running: bar.visible
        repeat: true
        onTriggered: {
            var target = ((bar.angle % 360) + 360) % 360
            var diff = ((target - bar.current + 180 + 360) % 360) - 180
            if (Math.abs(diff) <= 0.1) {
                if (bar.current !== target) {
                    bar.current = target
                    bar.requestPaint()
                }
                return
            }
            var damp = Math.abs(diff) > 30 ? 0.18 : 0.12
            bar.current = (bar.current + diff * damp + 360) % 360
            bar.requestPaint()
        }
    }

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        var W = width
        var H = height
        ctx.clearRect(0, 0, W, H)

        var tapeW = Math.min(W * 0.50, 900)
        var tapeX = (W - tapeW) / 2
        var tapeR = tapeX + tapeW
        var px = tapeW / 180
        var CENTER = W / 2
        var visibleRange = Math.ceil(tapeW / px / 2)
        var startAngle = Math.floor(bar.current - visibleRange)
        var tickTop = H * 0.30
        var tickBot = H * 0.50

        ctx.fillStyle = "rgba(8, 28, 46, 0.16)"
        ctx.beginPath()
        ctx.rect(tapeX - 8, H * 0.18, tapeW + 16, H * 0.62)
        ctx.fill()

        ctx.strokeStyle = "rgba(155, 220, 240, 0.45)"
        ctx.lineWidth = 1.2
        ctx.beginPath()
        ctx.moveTo(tapeX - 86, H * 0.22)
        ctx.lineTo(tapeX - 16, H * 0.22)
        ctx.lineTo(tapeX - 16, H * 0.56)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(tapeR + 86, H * 0.22)
        ctx.lineTo(tapeR + 16, H * 0.22)
        ctx.lineTo(tapeR + 16, H * 0.56)
        ctx.stroke()

        ctx.strokeStyle = "rgba(160, 228, 245, 0.28)"
        ctx.beginPath()
        ctx.moveTo(tapeX, tickBot + 3)
        ctx.lineTo(tapeR, tickBot + 3)
        ctx.stroke()

        ctx.textAlign = "center"
        ctx.textBaseline = "middle"
        for (var i = 0; i <= visibleRange * 2; i++) {
            var deg = (startAngle + i + 360) % 360
            var x = CENTER + (startAngle + i - bar.current) * px
            if (x < tapeX || x > tapeR)
                continue
            if (deg % 30 === 0) {
                ctx.strokeStyle = "rgba(220, 248, 255, 0.92)"
                ctx.lineWidth = 1.8
                ctx.beginPath()
                ctx.moveTo(x, tickTop - 2)
                ctx.lineTo(x, tickBot)
                ctx.stroke()
                ctx.font = "600 13px Bahnschrift"
                ctx.fillStyle = "rgba(240, 252, 255, 0.92)"
                ctx.fillText(String(deg), x, H * 0.70)
            } else if (deg % 10 === 0) {
                ctx.strokeStyle = "rgba(180, 230, 250, 0.55)"
                ctx.lineWidth = 1.2
                ctx.beginPath()
                ctx.moveTo(x, tickTop + 4)
                ctx.lineTo(x, tickBot - 4)
                ctx.stroke()
            } else if (deg % 5 === 0) {
                ctx.strokeStyle = "rgba(160, 220, 240, 0.28)"
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(x, tickTop + 10)
                ctx.lineTo(x, tickBot - 8)
                ctx.stroke()
            } else {
                ctx.strokeStyle = "rgba(150, 210, 230, 0.12)"
                ctx.lineWidth = 0.8
                ctx.beginPath()
                ctx.moveTo(x, tickTop + 14)
                ctx.lineTo(x, tickBot - 12)
                ctx.stroke()
            }
        }

        var cx = CENTER
        ctx.fillStyle = "rgba(157, 231, 245, 0.22)"
        ctx.beginPath()
        ctx.moveTo(cx, 4)
        ctx.lineTo(cx - 14, 28)
        ctx.lineTo(cx + 14, 28)
        ctx.closePath()
        ctx.fill()
        ctx.fillStyle = Theme.cyan
        ctx.beginPath()
        ctx.moveTo(cx, 8)
        ctx.lineTo(cx - 10, 26)
        ctx.lineTo(cx + 10, 26)
        ctx.closePath()
        ctx.fill()
        ctx.strokeStyle = "rgba(255, 255, 255, 0.65)"
        ctx.lineWidth = 1
        ctx.stroke()

        ctx.strokeStyle = "rgba(180, 240, 255, 0.7)"
        ctx.lineWidth = 1.2
        ctx.beginPath()
        ctx.moveTo(cx, 26)
        ctx.lineTo(cx, tickBot)
        ctx.stroke()
    }
}
