import QtQuick
import QtQuick.Controls

// Header button of the redesigned UI ("Mes PC", "Réglages", Multi-écran, light/dark): rounded,
// framed by the theme, icon + optional label, optional switch.
AbstractButton {
    id: pill

    property string iconName: ""
    property bool showSwitch: false
    property bool switchOn: false
    property string badge: ""

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    implicitHeight: 50
    implicitWidth: row.implicitWidth + (text.length > 0 ? 36 : 28)

    background: Rectangle {
        radius: Theme.pill
        color: pill.down ? Theme.surface2 : Theme.surface
        border.width: Math.max(1, Theme.border)
        border.color: pill.activeFocus && Theme.showFocus ? Theme.accent
                    : (pill.hovered ? Qt.alpha(Theme.accent, 0.6) : Theme.borderColor)
        Behavior on border.color { ColorAnimation { duration: Theme.animMs } }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 10

            Item {
                width: 22; height: 22
                visible: pill.iconName.length > 0
                anchors.verticalCenter: parent.verticalCenter
                LavIcon { anchors.fill: parent; name: pill.iconName; color: Theme.fg }
                Text {
                    visible: pill.badge.length > 0
                    text: pill.badge
                    color: Theme.fg
                    font.pixelSize: 10
                    font.bold: true
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: -6
                    anchors.bottomMargin: -6
                }
            }
            Text {
                visible: pill.text.length > 0
                text: pill.text
                color: Theme.fg
                font.family: Theme.textFont
                font.pixelSize: 18
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                visible: pill.showSwitch
                width: 40; height: 22; radius: 11
                anchors.verticalCenter: parent.verticalCenter
                color: pill.switchOn ? Theme.on : Theme.off
                Rectangle {
                    width: 16; height: 16; radius: 8
                    y: 3
                    x: pill.switchOn ? parent.width - width - 3 : 3
                    color: pill.switchOn ? Theme.onAccent : Theme.fg
                    Behavior on x { NumberAnimation { duration: Theme.animMs } }
                }
            }
        }
    }
}
