import QtQuick 2.15
import "../HudTheme.js" as Theme

Item {
    id: panel
    property alias radius: surface.radius

    GlassSurface {
        id: surface
        anchors.fill: parent
        radius: 16
    }
}
