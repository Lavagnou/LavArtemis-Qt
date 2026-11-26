import QtQuick
import QtQuick.Controls

// Button of the home header (prototype: mode, Multi-écran, Mes PC, Réglages): h40, pill radius,
// the theme's border, surface, 14/700, icon 18. Mes PC and Réglages are in the focus grid (`fid`);
// the mode button and Multi-écran are not. Prototype px x k.
Rectangle {
    id: btn

    property var home: null
    property string fid: ""
    property real k: 1.25
    property string text: ""
    property string iconPath: ""
    property real iconStroke: 1.8
    property bool showSwitch: false
    property bool switchOn: false
    property string badge: ""
    property string tip: ""

    signal clicked()

    readonly property bool focused: home !== null && fid !== "" && home.isFocused(fid)
    readonly property bool padRing: focused && Theme.showFocus
    readonly property bool mouseRing: focused && Theme.input === "mouse" && !Theme.affiche
    readonly property bool fill: Theme.affiche && focused && (Theme.showFocus || Theme.input === "mouse")

    height: 40 * k
    width: text === "" ? 40 * k : row.width + (showSwitch ? 24 : 28) * k
    radius: Theme.pill * k
    color: fill ? Theme.ringFillColor : Theme.surface
    border.width: Theme.border * k
    border.color: Theme.borderColor
    scale: padRing && !Theme.affiche ? Theme.focusScale : 1.0
    Behavior on scale { NumberAnimation { duration: 150 } }
    transform: Translate { y: btn.mouseRing ? -5 * btn.k : 0; Behavior on y { NumberAnimation { duration: 150 } } }

    readonly property color ink: fill ? Theme.ringFillFg : Theme.fg

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        x: btn.text === "" ? (parent.width - width) / 2 : (btn.showSwitch ? 14 : 14) * btn.k
        spacing: 8 * btn.k
        LavIcon {
            visible: btn.iconPath !== ""
            path: btn.iconPath
            size: 18 * btn.k
            stroke: btn.iconStroke
            color: btn.ink
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: btn.text !== ""
            text: btn.text
            color: btn.ink
            font.family: Theme.textFont
            font.pixelSize: 14 * btn.k
            font.weight: Font.Bold
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle {
            visible: btn.showSwitch
            width: 32 * btn.k; height: 18 * btn.k; radius: 9 * btn.k
            anchors.verticalCenter: parent.verticalCenter
            color: btn.switchOn ? Theme.on : Theme.off
            Rectangle {
                width: 14 * btn.k; height: width; radius: width / 2
                y: 2 * btn.k
                x: btn.switchOn ? parent.width - width - 2 * btn.k : 2 * btn.k
                color: "#FFFFFF"
                Behavior on x { NumberAnimation { duration: Theme.animMs } }
            }
        }
    }
    Text {
        visible: btn.badge !== ""
        text: btn.badge
        color: Theme.muted
        font.family: Theme.textFont
        font.pixelSize: 9 * btn.k
        font.weight: Font.ExtraBold
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 3 * btn.k
        anchors.bottomMargin: 2 * btn.k
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Theme.hoverRingWidth * btn.k
        radius: btn.radius + Theme.hoverRingWidth * btn.k
        color: "transparent"
        border.width: Theme.hoverRingWidth * btn.k
        border.color: Theme.hoverRing
        visible: btn.mouseRing
    }
    FocusRing { active: btn.padRing || (Theme.affiche && btn.focused && Theme.input === "mouse"); radius: btn.radius; k: btn.k }

    MouseArea {
        id: hit
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: if (btn.home && btn.fid !== "") btn.home.hover(btn.fid)
        onClicked: btn.clicked()
    }
    ToolTip.visible: tip !== "" && hit.containsMouse
    ToolTip.text: tip
    ToolTip.delay: 500
}
