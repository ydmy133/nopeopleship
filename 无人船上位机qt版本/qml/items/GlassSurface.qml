import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: surface
    property bool danger: false
    property bool active: false
    property bool hovered: false
    property real radius: 18
    property bool pill: false

    readonly property real r: pill ? height * 0.5 : radius

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 5
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        radius: surface.r
        color: "#3A061018"
        opacity: 0.45
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: surface.r + 2
        color: "transparent"
        border.width: 3
        border.color: surface.danger ? "#33E07068" : (surface.active ? "#4488D4EE" : "#2288C8E0")
    }

    Rectangle {
        id: body
        anchors.fill: parent
        radius: surface.r
        border.width: 1
        border.color: surface.danger
                       ? (surface.active || surface.hovered ? "#C8E09088" : "#96D08078")
                       : (surface.active ? "#D0D8F4FF" : (surface.hovered ? "#B8D4ECFF" : Theme.borderSoft))
        antialiasing: true
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: surface.danger ? "#4A3A2430" : "#4A244860"
            }
            GradientStop {
                position: 0.45
                color: surface.danger ? "#3A241820" : "#38203850"
            }
            GradientStop {
                position: 1.0
                color: "#55101424"
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.min(36, parent.height * 0.46)
            radius: body.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#38FFFFFF" }
                GradientStop { position: 1.0; color: "#00FFFFFF" }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(0, body.radius - 1)
            color: "transparent"
            border.color: "#22FFFFFF"
        }
    }
}
