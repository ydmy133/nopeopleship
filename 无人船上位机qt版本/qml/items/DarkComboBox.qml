import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import "../HudTheme.js" as Theme

ComboBox {
    id: control
    font.pixelSize: 15
    leftPadding: 10
    rightPadding: 28

    contentItem: Text {
        leftPadding: control.leftPadding
        rightPadding: control.indicator.width + 10
        text: control.displayText
        font: control.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 320
        implicitHeight: 32
        radius: 6
        color: control.pressed ? "#050505" : "#0a0a0a"
        border.color: (control.pressed || control.popup.visible) ? Theme.cyan : Theme.borderSoft
        border.width: 1
    }

    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "▾"
        color: Theme.textDim
        font.pixelSize: 12
    }

    delegate: ItemDelegate {
        width: control.width
        height: 32
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: modelData
            color: Theme.text
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
        }
        background: Rectangle {
            color: highlighted ? "#222222" : "#0a0a0a"
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        padding: 1
        transformOrigin: Item.TopLeft
        scale: (Window.window && Window.window.uiScaleX > 0) ? Window.window.uiScaleX : 1
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }
        background: Rectangle {
            color: "#0a0a0a"
            border.color: Theme.borderSoft
            radius: 6
        }
    }
}
