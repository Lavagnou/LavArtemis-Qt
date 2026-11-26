import QtQuick
import QtQuick.Controls

// « Mise à jour » next to the wordmark when a new version is out (prototype header): h36, pill,
// the theme's border, 13/700, update icon 16. Ludique: a small lime pill (h24, 12/800, icon 13).
Rectangle {
    id: up
    property real k: 1.25
    readonly property bool lud: Theme.ludique
    visible: typeof window !== "undefined" && window.updateVersion !== ""
    height: (lud ? 24 : 36) * k
    width: upRow.width + (lud ? 20 : 24) * k + 2 * border.width
    radius: lud ? height / 2 : Theme.pill * k
    color: lud ? "#C6F432" : Theme.surface
    border.width: lud ? 0 : Theme.border * k
    border.color: Theme.borderColor
    Row {
        id: upRow
        anchors.centerIn: parent
        spacing: (up.lud ? 5 : 7) * up.k
        LavIcon { path: Theme.icons.update; size: (up.lud ? 13 : 16) * up.k; stroke: 2; color: up.lud ? "#1B1530" : Theme.fg; anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: qsTr("Mise à jour")
            color: up.lud ? "#1B1530" : Theme.fg
            font.family: Theme.textFont
            font.pixelSize: (up.lud ? 12 : 13) * up.k
            font.weight: up.lud ? Font.ExtraBold : Font.Bold
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    MouseArea { id: upHit; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: window.openUpdateDialog() }
    ToolTip.visible: upHit.containsMouse
    ToolTip.text: typeof window !== "undefined" ? qsTr("LavArtemis %1 est disponible").arg(window.updateVersion) : ""
}
