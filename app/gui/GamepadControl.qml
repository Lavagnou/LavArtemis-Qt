import QtQuick 2.9
import QtQuick.Controls 2.2

// One bindable control on the mapping diagram: click it, then operate the
// matching control on the physical device.
//
// State is derived from the owning ControllerDiagram rather than pushed in,
// so an instance is just its geometry and the elements it binds.
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

    readonly property bool bound: diagram ? diagram.isBound(elements) : false
    readonly property bool selected: diagram ? diagram.isSelected(elements) : false
    // How hard it reads right now, 0..1. Drives the highlight, and for "bar"
    // the fill height -- which is how a trigger stuck at half travel becomes
    // obvious here instead of being discovered in a game.
    readonly property real level: diagram ? diagram.levelFor(elements) : 0
    readonly property string sourceText: diagram ? diagram.sourceFor(elements) : ""

    readonly property color idleColor: "#555555"
    readonly property color boundColor: "#60c060"
    readonly property color selectedColor: "#e0b040"
    readonly property color activeColor: "#00cccc"

    readonly property color stateColor: selected ? selectedColor
                                                 : (bound ? boundColor : idleColor)
    readonly property real litness: shape === "bar" ? 0 : Math.min(1, Math.abs(level))

    // Pulses the outline while this control is the one being waited on.
    property real pulse: 1
    onSelectedChanged: if (!selected) pulse = 1

    SequentialAnimation on pulse {
        running: root.selected
        loops: Animation.Infinite
        NumberAnimation { from: 1.0; to: 0.3; duration: 550; easing.type: Easing.InOutQuad }
        NumberAnimation { from: 0.3; to: 1.0; duration: 550; easing.type: Easing.InOutQuad }
    }

    function mixColor(from, to, t) {
        return Qt.rgba(from.r + (to.r - from.r) * t,
                       from.g + (to.g - from.g) * t,
                       from.b + (to.b - from.b) * t,
                       1.0)
    }

    Rectangle {
        anchors.fill: parent
        radius: {
            if (root.shape === "circle" || root.shape === "ring") return width / 2
            if (root.shape === "pill") return height / 2
            return 6
        }
        color: root.shape === "ring"
               ? "transparent"
               : root.mixColor(Qt.darker(root.stateColor, 3.4), root.activeColor, root.litness)
        border.color: Qt.rgba(root.stateColor.r, root.stateColor.g, root.stateColor.b, root.pulse)
        border.width: root.selected ? 3 : 2
        antialiasing: true

        // Trigger fill, anchored to the bottom so it reads like a gauge.
        Rectangle {
            visible: root.shape === "bar"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 3
            height: Math.max(0, Math.min(1, root.level)) * (parent.height - 6)
            radius: 4
            color: root.activeColor
            opacity: 0.75
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.label !== "" && root.shape !== "ring"
        text: root.label
        color: root.litness > 0.55 ? "#101010" : "#dddddd"
        font.pointSize: 9
        font.bold: true
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
                      ? qsTr("%1 — bound to %2. Click to remap, right-click to clear.")
                          .arg(root.label).arg(root.sourceText)
                      : qsTr("%1 — not bound. Click it, then operate it on your controller.")
                          .arg(root.label)
    }
}
