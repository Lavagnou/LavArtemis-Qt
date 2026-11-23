import QtQuick
import QtQuick.Controls

// Title row of a secondary page (Mes PC, Réglages): back button + big title.
Item {
    id: header
    property string title: ""
    signal back()
    width: parent ? parent.width : 0
    height: 64

    Row {
        anchors.verticalCenter: parent.verticalCenter
        spacing: 18
        RoundIconButton {
            iconName: "back"
            implicitWidth: 48; implicitHeight: 48
            anchors.verticalCenter: parent.verticalCenter
            onClicked: header.back()
        }
        Text {
            text: Theme.displayText(header.title)
            color: Theme.fg
            font.family: Theme.displayFont
            font.pixelSize: 40
            font.weight: Theme.displayWeight
            font.letterSpacing: Theme.tracking * 40
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
