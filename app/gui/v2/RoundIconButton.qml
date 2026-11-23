import QtQuick
import QtQuick.Controls

// Small round button (host switcher arrows, back).
AbstractButton {
    id: button
    property string iconName: "chevron-left"
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    implicitWidth: 40
    implicitHeight: 40
    opacity: enabled ? 1.0 : 0.35
    background: Rectangle {
        radius: Theme.id === "affiche" ? 0 : width / 2
        color: button.hovered ? Theme.surface2 : "transparent"
        border.width: Math.max(1, Theme.border)
        border.color: button.activeFocus && Theme.showFocus ? Theme.accent : Theme.borderColor
    }
    contentItem: Item {
        LavIcon { anchors.centerIn: parent; name: button.iconName; size: 18; color: Theme.fg }
    }
}
