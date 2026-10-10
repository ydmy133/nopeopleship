import QtQuick 2.15
import QtQuick.Window 2.15
import "items"
import "dialogs"
import "HudTheme.js" as Theme

Item {
    id: hud
    focus: true
    signal openSettings()

    property bool showPresets: false
    property bool showCruise: false
    property bool showAlerts: false
    property bool showReturn: false
    property bool mapInteractive: true
    readonly property bool overlayOpen: showPresets || showCruise || showAlerts || showReturn

    function t(zh, en) {
        return Theme.tr(usv.settings.language, zh, en)
    }

    function modeLabel() {
        var m = usv.control.activeMode
        if (m === "manual")
            return "Manual"
        if (m === "cruise_speed")
            return t("定速巡航", "Cruise")
        if (m === "cruise_dir")
            return t("定向巡航", "Heading")
        if (m === "track_hold")
            return t("航迹保持", "Track")
        if (m === "stable_anchor")
            return t("定点稳泊", "Hold")
        if (m === "navigate")
            return t("导航", "Navigate")
        return m
    }

    function returnSubtitle() {
        if (usv.control.oneKeyReturnActive)
            return t("正在返回设定点", "Returning now")
        if (usv.returnPoints.count > 0 || usv.settings.hasOneKeyPoint)
            return t("选择或保存返航点", "Select or save a point")
        return t("保存当前点后可返航", "Save a return point")
    }

    function stopSubtitle() {
        if (usv.control.navigating || usv.control.manualMoveActive || usv.control.manualSteerActive)
            return t("点击停止当前任务", "Stop current mission")
        return t("当前航行任务已停止", "Mission stopped")
    }

    function cruiseSubtitle() {
        var move = usv.telemetry.online && isFinite(usv.telemetry.outputMoveSpeed)
                ? Math.round(usv.telemetry.outputMoveSpeed) + "%" : "--"
        var steer = usv.telemetry.online && isFinite(usv.telemetry.outputSteerSpeed)
                ? Math.round(usv.telemetry.outputSteerSpeed) + "%" : "--"
        var parts = []
        if (usv.control.cruiseSpeedOn)
            parts.push(t("定速巡航", "Speed cruise"))
        if (usv.control.headingCruiseOn)
            parts.push(t("定向巡航", "Heading cruise"))
        if (usv.control.trackHoldOn)
            parts.push(t("航迹保持", "Track hold"))
        var speeds = t("行驶 " + move + " / 转向 " + steer,
                       "Drive " + move + " / Steer " + steer)
        if (parts.length > 0)
            return parts.join(" / ") + " · " + speeds
        return speeds
    }

    function navSubtitle() {
        if (usv.control.navigating && usv.control.navStatusLine.length > 0)
            return usv.control.navStatusLine
        return t("选择设置目标点", "Set a target point")
    }

    function bdsConnected() {
        return usv.telemetry.online && usv.telemetry.hasFix
    }

    function bdsLabel() {
        if (!usv.telemetry.online)
            return t("北斗未连接", "BDS Offline")
        if (!usv.telemetry.hasFix)
            return t("北斗无定位", "BDS No Fix")
        return t("北斗已连接", "BDS Online")
    }

    function batteryLabel() {
        var v = usv.telemetry.batteryLevel
        if (v <= 0)
            return "--"
        if (v <= 100)
            return Math.round(v) + "%"
        return v.toFixed(1) + "V"
    }

    function batteryPct() {
        var v = usv.telemetry.batteryLevel
        if (v <= 0)
            return 0
        if (v <= 1)
            return v * 100
        if (v <= 100)
            return v
        return Math.max(0, Math.min(100, (v - 22.0) / 7.2 * 100))
    }

    function applyMapWheel(dy) {
        if (dy > 0)
            usv.map.zoomBy(1)
        else if (dy < 0)
            usv.map.zoomBy(-1)
    }

    WheelHandler {
        enabled: hud.mapInteractive
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        grabPermissions: PointerHandler.TakeOverForbidden
        onWheel: function(event) {
            var dy = event.angleDelta.y
            if (dy === 0)
                dy = event.pixelDelta.y
            hud.applyMapWheel(dy)
            event.accepted = (dy !== 0)
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 0
        enabled: hud.mapInteractive
        acceptedButtons: Qt.LeftButton
        property real lastX: 0
        property real lastY: 0
        onPressed: {
            lastX = mouse.x
            lastY = mouse.y
            usv.map.beginPan()
        }
        onPositionChanged: {
            if (!pressed)
                return
            var sx = (Window.window && hud.width > 0) ? (Window.window.width / hud.width) : 1
            var sy = (Window.window && hud.height > 0) ? (Window.window.height / hud.height) : 1
            usv.map.panBy((mouse.x - lastX) * sx, (mouse.y - lastY) * sy)
            lastX = mouse.x
            lastY = mouse.y
        }
        onReleased: usv.map.endPan()
        onCanceled: usv.map.endPan()
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 88
        z: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#33081420" }
            GradientStop { position: 1.0; color: "#00081420" }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 140
        z: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00081420" }
            GradientStop { position: 1.0; color: "#33081420" }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: showAlerts && usv.settings.flash
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#ccff0000" }
            GradientStop { position: 0.5; color: "#00ff0000" }
            GradientStop { position: 1.0; color: "#ccff0000" }
        }
        SequentialAnimation on opacity {
            running: showAlerts && usv.settings.flash
            loops: Animation.Infinite
            NumberAnimation { from: 0.5; to: 0.1; duration: 800 }
            NumberAnimation { from: 0.1; to: 0.5; duration: 800 }
        }
        z: 1
    }

    HeadingBar {
        id: headingBar
        width: parent.width
        angle: usv.telemetry.heading
        z: 10
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.top: parent.top
        anchors.topMargin: 12
        spacing: 8
        z: 20

        Item {
            width: Math.max(statusRow.width, bdsRow.width) + 28
            height: 40

            GlassSurface {
                anchors.fill: parent
                pill: true
            }

            Row {
                id: statusRow
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                spacing: 10

                Rectangle {
                    width: 22
                    height: 22
                    radius: 6
                    color: "#3328A0C0"
                    border.color: Theme.cyan
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter
                    HudIcon {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        name: "logo"
                        stroke: Theme.cyan
                    }
                }

                Text {
                    text: usv.linkStatus
                    color: usv.linkUp ? Theme.ok : "#E2C56A"
                    font.family: Theme.fontUi
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    width: 1
                    height: 10
                    color: "#55FFFFFF"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: hud.modeLabel()
                    color: Theme.text
                    font.family: Theme.fontNum
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Item {
            width: Math.max(statusRow.width, bdsRow.width) + 28
            height: 36

            GlassSurface {
                anchors.fill: parent
                pill: true
            }

            Row {
                id: bdsRow
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                spacing: 10

                Rectangle {
                    width: 22
                    height: 22
                    radius: 6
                    color: "#3328A0C0"
                    border.color: hud.bdsConnected() ? Theme.ok : Theme.warn
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter
                    HudIcon {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        name: "satellite"
                        stroke: hud.bdsConnected() ? Theme.ok : Theme.warn
                    }
                }

                Rectangle {
                    width: 7
                    height: 7
                    radius: 4
                    color: hud.bdsConnected() ? Theme.ok : Theme.warn
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: hud.bdsLabel()
                    color: hud.bdsConnected() ? Theme.ok : "#E2C56A"
                    font.family: Theme.fontUi
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.top: parent.top
        anchors.topMargin: 14
        spacing: 14
        z: 20

        Item {
            width: 26
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            Row {
                spacing: 3
                anchors.bottom: parent.bottom
                Repeater {
                    model: 4
                    Rectangle {
                        width: 4
                        height: 5 + index * 3
                        radius: 1
                        color: index < hud.signalBars() ? Theme.cyan : "#33FFFFFF"
                    }
                }
            }
        }

        Row {
            spacing: 7
            anchors.verticalCenter: parent.verticalCenter
            Item {
                width: 26
                height: 13
                anchors.verticalCenter: parent.verticalCenter
                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    color: "transparent"
                    border.color: Theme.cyan
                    border.width: 1
                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.top: parent.top
                        anchors.topMargin: 2
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        width: Math.max(2, (parent.width - 7) * hud.batteryPct() / 100)
                        radius: 1
                        color: hud.batteryPct() < 20 ? Theme.danger : Theme.cyan
                    }
                    Rectangle {
                        width: 2
                        height: 5
                        radius: 1
                        color: Theme.cyan
                        anchors.left: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
            Text {
                text: hud.batteryLabel()
                color: Theme.textDim
                font.family: Theme.fontNum
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Item {
        visible: usv.showBdsBanner
        anchors.top: parent.top
        anchors.topMargin: 102
        anchors.right: parent.right
        anchors.rightMargin: 24
        z: 20
        width: 168
        height: 32
        GlassSurface {
            anchors.fill: parent
            pill: true
        }
        Row {
            anchors.centerIn: parent
            spacing: 8
            Rectangle {
                width: 7
                height: 7
                radius: 4
                color: usv.telemetry.online ? Theme.ok : Theme.warn
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: usv.telemetry.online ? t("北斗在线", "BDS Online") : t("北斗离线", "BDS Offline")
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: 12
            }
        }
    }

    Item {
        visible: usv.control.navigating && usv.control.navStatusLine.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 96
        z: 15
        width: navText.implicitWidth + 28
        height: 30
        GlassSurface {
            anchors.fill: parent
            pill: true
        }
        Text {
            id: navText
            anchors.centerIn: parent
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: 13
            text: usv.control.navStatusLine
        }
    }

    ControlPad {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -36
        z: 9
    }

    CompassRose {
        heading: usv.telemetry.heading
        anchors.left: parent.left
        anchors.leftMargin: 72
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 52
        z: 9
    }

    TelemetryHud {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 46
        z: 9
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 44
        anchors.top: parent.top
        anchors.topMargin: 168
        spacing: 16
        z: 5

        GlassCard {
            title: t("返航", "Return")
            subtitle: hud.returnSubtitle()
            icon: "pin"
            active: hud.showReturn || usv.control.oneKeyReturnActive
            onClicked: showReturn = !showReturn
        }
        GlassCard {
            title: t("停止", "Stop")
            subtitle: hud.stopSubtitle()
            icon: "stop"
            danger: true
            onClicked: usv.control.emergencyStop()
        }
    }

    Column {
        anchors.right: parent.right
        anchors.rightMargin: 44
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14
        z: 5

        GlassCard {
            title: t("点到点导航", "Point to point")
            subtitle: hud.navSubtitle()
            icon: "nav"
            active: hud.showPresets || (usv.control.navigating && !usv.control.oneKeyReturnActive)
            onClicked: showPresets = !showPresets
        }
        GlassCard {
            title: t("速度模式", "Speed mode")
            subtitle: hud.cruiseSubtitle()
            icon: "speed"
            active: hud.showCruise || usv.control.cruiseSpeedOn || usv.control.headingCruiseOn
            onClicked: showCruise = !showCruise
        }
        GlassCard {
            title: t("定点稳泊", "Station keeping")
            subtitle: usv.control.autoHolding ? t("稳泊中", "Holding") : t("保持当前位置", "Hold position")
            icon: "anchor"
            active: usv.control.autoHolding
            onClicked: usv.control.toggleAutoHolding()
        }
        GlassCard {
            title: t("风险警报", "Risk alerts")
            subtitle: t("查看潜在风险", "View potential risks")
            icon: "alert"
            active: hud.showAlerts
            onClicked: showAlerts = !showAlerts
        }
        GlassCard {
            title: t("配置文件", "Settings")
            subtitle: t("通信与系统参数", "Link & system")
            icon: "gear"
            onClicked: hud.openSettings()
        }
    }

    ReturnDialog {
        visible: showReturn
        anchors.fill: parent
        z: 50
        onRequestClose: showReturn = false
    }

    PresetDialog {
        visible: showPresets
        anchors.fill: parent
        z: 50
        onRequestClose: showPresets = false
    }

    CruiseDialog {
        visible: showCruise
        anchors.fill: parent
        z: 50
        onRequestClose: showCruise = false
    }

    AlertDialog {
        visible: showAlerts
        anchors.fill: parent
        z: 50
        onRequestClose: showAlerts = false
    }

    Keys.onPressed: function(event) {
        if (event.isAutoRepeat)
            return
        if (event.key === Qt.Key_W || event.key === Qt.Key_Up) {
            usv.control.controlDown("forward"); event.accepted = true
        } else if (event.key === Qt.Key_S || event.key === Qt.Key_Down) {
            usv.control.controlDown("backward"); event.accepted = true
        } else if (event.key === Qt.Key_A || event.key === Qt.Key_Left) {
            usv.control.controlDown("turnLeft"); event.accepted = true
        } else if (event.key === Qt.Key_D || event.key === Qt.Key_Right) {
            usv.control.controlDown("turnRight"); event.accepted = true
        } else if (event.key === Qt.Key_Space) {
            usv.control.emergencyStop(); event.accepted = true
        }
    }
    Keys.onReleased: function(event) {
        if (event.isAutoRepeat)
            return
        if (event.key === Qt.Key_W || event.key === Qt.Key_Up) {
            usv.control.controlUp("forward"); event.accepted = true
        } else if (event.key === Qt.Key_S || event.key === Qt.Key_Down) {
            usv.control.controlUp("backward"); event.accepted = true
        } else if (event.key === Qt.Key_A || event.key === Qt.Key_Left) {
            usv.control.controlUp("turnLeft"); event.accepted = true
        } else if (event.key === Qt.Key_D || event.key === Qt.Key_Right) {
            usv.control.controlUp("turnRight"); event.accepted = true
        }
    }

    Component.onCompleted: forceActiveFocus()
}
