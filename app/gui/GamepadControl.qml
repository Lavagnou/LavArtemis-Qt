import QtQuick
import QtQuick.Controls
import "v2"

// One bindable control on the mapping diagram: click it, then operate the
// matching control on the physical device.
//
// State is derived from the owning ControllerDiagram rather than pushed in,
// so an instance is just its geometry and the elements it binds.
// Looks of the prototype's mapper (V8-46, 47): not bound = surface with a 2 px dashed muted border; bound =
// accent; waited on = warnBg with a 3 px warnLine border, 1.08 larger. Live input lights the control.
Item {
    id: root

    // The ControllerDiagram this control belongs to. It owns the binding and
    // live-value maps and the two request signals.
    property var diagram: null

    // SDL element names this control binds, in capture order. One for a
    // button, two for a stick's axis pair.
    property var elements: []
    property string label: ""

    // "circle" | "pill" | "wedge" | "bar" | "ring"
    property string shape: "circle"
    // Corner of "bar" and "wedge" (prototype r)
    property real corner: 8

    readonly property bool bound: diagram ? diagram.isBound(elements) : false
    readonly property bool selected: diagram ? diagram.isSelected(elements) : false
    // How hard it reads right now, 0..1. Drives the highlight, and for "bar"
    // the fill height -- which is how a trigger stuck at half travel becomes
    // obvious here instead of being discovered in a game.
    readonly property real level: diagram ? diagram.levelFor(elements) : 0
    readonly property string sourceText: diagram ? diagram.sourceFor(elements) : ""

    // Live input: the theme's « on » colour over the control
    readonly property color activeColor: Theme.ludique ? "#C6F432" : "#00CCCC"
    readonly property real litness: shape === "bar" ? 0 : Math.min(1, Math.abs(level))
    readonly property real cornerRadius: shape === "circle" || shape === "ring" ? width / 2
                                       : shape === "pill" ? height / 2 : corner
    readonly property color fill: selected ? Theme.warnBg : (bound ? Theme.accent : Theme.surface)
    readonly property color ink: selected ? Theme.fg : (bound ? Theme.onAccent : Theme.fg)

    scale: selected ? 1.08 : 1
    Behavior on scale { NumberAnimation { duration: 150 } }

    // Not bound: the dashed outline
    DashedRect {
        anchors.fill: parent
        visible: !root.bound && !root.selected
        radius: root.cornerRadius
        borderWidth: 2
        borderColor: Theme.muted
        fillColor: Theme.surface
    }
    Rectangle {
        anchors.fill: parent
        visible: root.bound || root.selected
        radius: root.cornerRadius
        color: root.fill
        border.color: Theme.warnLine
        border.width: root.selected ? 3 : 0
        antialiasing: true
    }
    // Live input over the control
    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.activeColor
        opacity: 0.85 * root.litness
        visible: opacity > 0
    }
    // Trigger fill, anchored to the bottom so it reads like a gauge.
    Rectangle {
        visible: root.shape === "bar"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 3
        height: Math.max(0, Math.min(1, root.level)) * (parent.height - 6)
        radius: Math.max(0, root.corner - 3)
        color: root.activeColor
        opacity: 0.75
    }

    Text {
        anchors.centerIn: parent
        visible: root.label !== "" && root.shape !== "ring"
        text: root.label
        color: root.litness > 0.55 ? "#101010" : root.ink
        font.family: Theme.textFont
        font.pixelSize: 12
        font.weight: Font.ExtraBold
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: function(mouse) {
            if (!root.diagram) {
                return
            }
            if (mouse.button === Qt.RightButton) {
                root.diagram.clearRequested(root.elements)
            }
            else {
                root.diagram.selectRequested(root.elements)
            }
        }

        ToolTip.delay: 600
        ToolTip.timeout: 5000
        ToolTip.visible: containsMouse
        ToolTip.text: root.bound
                      ? qsTr("%1 : assigné à %2. Clic pour le réassigner, clic droit pour l’effacer.")
                          .arg(root.label).arg(root.sourceText)
                      : qsTr("%1 : pas assigné. Clique-le, puis actionne-le sur ta manette.")
                          .arg(root.label)
    }
}
