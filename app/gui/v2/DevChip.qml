import QtQuick
import SystemProperties 1.0

// « DEV » next to the wordmark (prototype): 11/800 .04em, pad 3 7, pill radius, badge colours.
Rectangle {
    property real k: 1.25
    visible: SystemProperties.isDevChannel
    width: devText.implicitWidth + 14 * k
    height: devText.implicitHeight + 6 * k
    radius: Theme.pill * k
    color: Theme.badgeBg
    Text {
        id: devText
        anchors.centerIn: parent
        text: "DEV"
        color: Theme.badgeFg
        font.family: Theme.textFont
        font.pixelSize: 11 * parent.k
        font.weight: Font.ExtraBold
        font.letterSpacing: 0.04 * 11 * parent.k
    }
}
