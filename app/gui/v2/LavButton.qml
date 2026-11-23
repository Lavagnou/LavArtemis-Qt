import QtQuick
import QtQuick.Controls

// Main action button (Reprendre, Lancer, Appairer): filled with the accent, or quiet.
AbstractButton {
    id: button

    property bool primary: true
    property string iconName: ""
    property string hint: ""   // gamepad/keyboard hint ("A", "Entrée"), shown only for that input

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    implicitHeight: 54
    implicitWidth: row.implicitWidth + 56

    background: Rectangle {
        radius: Theme.id === "affiche" ? 0 : height / 2
        color: button.primary ? Theme.accent : Theme.surface2
        border.width: Theme.id === "affiche" ? 3 : 0
        border.color: Theme.fg
        scale: button.down ? 0.97 : (button.hovered && Theme.input === "mouse" ? 1.03 : 1.0)
        Behavior on scale { NumberAnimation { duration: Theme.animMs } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -5
            radius: parent.radius + 5
            color: "transparent"
            border.width: 3
            border.color: Theme.fg
            visible: button.activeFocus && Theme.showFocus
        }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 12
            Rectangle {
                visible: button.hint.length > 0 && Theme.showFocus
                width: hintText.implicitWidth + 12; height: 24; radius: 12
                anchors.verticalCenter: parent.verticalCenter
                color: button.primary ? Theme.onAccent : Theme.fg
                Text {
                    id: hintText
                    anchors.centerIn: parent
                    text: button.hint
                    color: button.primary ? Theme.accent : Theme.bg
                    font.pixelSize: 13
                    font.bold: true
                }
            }
            LavIcon {
                visible: button.iconName.length > 0
                name: button.iconName
                size: 18
                color: button.primary ? Theme.onAccent : Theme.fg
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: Theme.displayText(button.text)
                color: button.primary ? Theme.onAccent : Theme.fg
                font.family: Theme.displayFont
                font.pixelSize: 22
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
