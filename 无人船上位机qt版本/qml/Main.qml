import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import "items"
import "HudTheme.js" as Theme

ApplicationWindow {
    id: root
    visible: true
    visibility: Window.FullScreen
    width: Screen.width
    height: Screen.height
    minimumWidth: Math.min(960, Screen.width)
    minimumHeight: Math.min(540, Screen.height)
    title: Theme.tr(usv.settings.language, "无人船控制系统", "USV Control")
    color: "#071828"
    flags: Qt.Window | Qt.FramelessWindowHint
    palette.window: "#0b1c28"
    palette.windowText: "#e8f7ff"
    palette.base: "#12283a"
    palette.text: "#e8f7ff"
    palette.button: "#16344c"
    palette.buttonText: "#e8f7ff"
    palette.highlight: "#2aa8d0"
    palette.highlightedText: "#ffffff"
    palette.mid: "#2a5068"

    readonly property real designWidth: 1920
    readonly property real designHeight: 1080
    readonly property real uiScaleX: {
        var s = width / designWidth
        return (s > 0 && s === s) ? s : 1
    }
    readonly property real uiScaleY: {
        var s = height / designHeight
        return (s > 0 && s === s) ? s : 1
    }
    readonly property real uiScale: Math.min(uiScaleX, uiScaleY)

    function toggleFullscreen() {
        if (visibility === Window.FullScreen)
            enterWindowed()
        else
            visibility = Window.FullScreen
    }

    function enterWindowed() {
        visibility = Window.Windowed
        Qt.callLater(fitWindowedGeometry)
    }

    function fitWindowedGeometry() {
        if (visibility === Window.FullScreen)
            return
        var aw = Math.max(1, Screen.desktopAvailableWidth)
        var ah = Math.max(1, Screen.desktopAvailableHeight)
        width = aw
        height = ah
        x = Screen.virtualX + Math.round((Screen.width - width) / 2)
        y = Screen.virtualY + Math.round((Screen.height - height) / 2)
    }

    Shortcut {
        sequence: "Ctrl+Alt+Q"
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (settingsLoader.active) {
                settingsLoader.active = false
            } else {
                root.toggleFullscreen()
            }
        }
    }
    Shortcut {
        sequence: "F11"
        onActivated: root.toggleFullscreen()
    }

    Image {
        anchors.fill: parent
        source: "qrc:/equ/background.png"
        fillMode: Image.PreserveAspectCrop
        z: 0
        visible: !usv.splashDone
    }

    MapBackground {
        anchors.fill: parent
        z: 0
        visible: usv.splashDone
    }

    Item {
        id: uiRoot
        width: root.designWidth
        height: root.designHeight
        x: 0
        y: 0
        transformOrigin: Item.TopLeft
        transform: Scale {
            xScale: root.uiScaleX
            yScale: root.uiScaleY
        }
        z: 1

        SplashView {
            anchors.fill: parent
            visible: !usv.splashDone
            z: 100
        }

        HudView {
            id: hudView
            anchors.fill: parent
            visible: usv.splashDone
            mapInteractive: !settingsLoader.active && !hudView.overlayOpen
            onOpenSettings: settingsLoader.active = true
        }

        Loader {
            id: settingsLoader
            anchors.fill: parent
            active: false
            z: 80
            sourceComponent: SettingsView {
                onBack: settingsLoader.active = false
            }
        }

        Toast {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 96
            z: 200
        }
    }
}
