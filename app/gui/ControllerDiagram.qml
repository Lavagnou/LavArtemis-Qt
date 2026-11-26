import QtQuick
import QtQuick.Shapes
import "v2"

// The controller, laid out as the prototype's mapper (planches V8-46, 47: a 760 x 400 schematic, the body in
// the theme's surface2 with a 3 px line, one control per element). Clicking a control asks for it to be bound;
// the whole thing animates from live input, so a wrong or inverted axis shows up here rather than in a game.
Item {
    id: root

    // element -> SDL source token
    property var bindings: ({})
    // element -> current value (-1..1 for sticks, 0..1 for everything else)
    property var values: ({})
    // element currently being waited on, "" when idle
    property string target: ""

    signal selectRequested(var elements)
    signal clearRequested(var elements)

    readonly property real designWidth: 760
    readonly property real designHeight: 400
    readonly property real scaleFactor: Math.min(width / designWidth, height / designHeight)

    function isBound(elements) {
        if (elements.length === 0) {
            return false
        }
        for (var i = 0; i < elements.length; i++) {
            if (bindings[elements[i]] === undefined) {
                return false
            }
        }
        return true
    }

    function isSelected(elements) {
        return target !== "" && elements.indexOf(target) >= 0
    }

    function levelFor(elements) {
        var peak = 0
        for (var i = 0; i < elements.length; i++) {
            var value = values[elements[i]]
            if (value !== undefined) {
                peak = Math.max(peak, Math.abs(value))
            }
        }
        return peak
    }

    function sourceFor(elements) {
        var parts = []
        for (var i = 0; i < elements.length; i++) {
            var source = bindings[elements[i]]
            if (source !== undefined) {
                parts.push(source)
            }
        }
        return parts.length > 0 ? parts.join(" / ") : qsTr("rien")
    }

    function axisValue(element) {
        var value = values[element]
        return value === undefined ? 0 : value
    }

    Item {
        id: board
        width: root.designWidth
        height: root.designHeight
        anchors.centerIn: parent
        scale: root.scaleFactor
        transformOrigin: Item.Center

        // The body (prototype path)
        Shape {
            anchors.fill: parent
            ShapePath {
                fillColor: Theme.surface2
                strokeColor: Theme.line
                strokeWidth: 3
                PathSvg {
                    path: "M190 96 Q232 70 300 76 L460 76 Q528 70 570 96 Q652 140 678 290 Q692 372 632 386 Q584 396 548 334 L512 286 L248 286 L212 334 Q176 396 128 386 Q68 372 82 290 Q108 140 190 96 Z"
                }
            }
        }

        // ---- Triggers and shoulders ---------------------------------------
        GamepadControl { diagram: root; elements: ["lefttrigger"]; label: qsTr("LT"); shape: "bar"; corner: 10; x: 196; y: 6; width: 96; height: 30 }
        GamepadControl { diagram: root; elements: ["righttrigger"]; label: qsTr("RT"); shape: "bar"; corner: 10; x: 468; y: 6; width: 96; height: 30 }
        GamepadControl { diagram: root; elements: ["leftshoulder"]; label: qsTr("LB"); shape: "pill"; x: 196; y: 44; width: 96; height: 26 }
        GamepadControl { diagram: root; elements: ["rightshoulder"]; label: qsTr("RB"); shape: "pill"; x: 468; y: 44; width: 96; height: 26 }

        // ---- Centre cluster -----------------------------------------------
        GamepadControl { diagram: root; elements: ["guide"]; label: "Guide"; shape: "pill"; x: 355; y: 84; width: 54; height: 34 }
        GamepadControl { diagram: root; elements: ["back"]; label: "Select"; shape: "pill"; x: 312; y: 130; width: 60; height: 26 }
        GamepadControl { diagram: root; elements: ["start"]; label: "Start"; shape: "pill"; x: 392; y: 130; width: 60; height: 26 }

        // ---- Face buttons -------------------------------------------------
        GamepadControl { diagram: root; elements: ["y"]; label: qsTr("Y"); x: 550; y: 104; width: 40; height: 40 }
        GamepadControl { diagram: root; elements: ["x"]; label: qsTr("X"); x: 506; y: 146; width: 40; height: 40 }
        GamepadControl { diagram: root; elements: ["b"]; label: qsTr("B"); x: 594; y: 146; width: 40; height: 40 }
        GamepadControl { diagram: root; elements: ["a"]; label: qsTr("A"); x: 550; y: 188; width: 40; height: 40 }

        // ---- D-pad --------------------------------------------------------
        GamepadControl { diagram: root; elements: ["dpup"]; label: "▲"; shape: "wedge"; corner: 8; x: 286; y: 186; width: 34; height: 34 }
        GamepadControl { diagram: root; elements: ["dpdown"]; label: "▼"; shape: "wedge"; corner: 8; x: 286; y: 254; width: 34; height: 34 }
        GamepadControl { diagram: root; elements: ["dpleft"]; label: "◀"; shape: "wedge"; corner: 8; x: 252; y: 220; width: 34; height: 34 }
        GamepadControl { diagram: root; elements: ["dpright"]; label: "▶"; shape: "wedge"; corner: 8; x: 320; y: 220; width: 34; height: 34 }

        // ---- Sticks -------------------------------------------------------
        // The circle binds the axis pair and the disc binds the click. The disc
        // is also what moves, so it is target and readout at once -- which is
        // what a stick physically is, and it keeps the centre unambiguous.
        Item {
            x: 196; y: 120; width: 76; height: 76
            GamepadControl { diagram: root; elements: ["leftx", "lefty"]; label: qsTr("Stick gauche"); shape: "ring"; anchors.fill: parent }
            GamepadControl {
                diagram: root; elements: ["leftstick"]; label: qsTr("L")
                width: 34; height: 34
                x: (parent.width - width) / 2 + root.axisValue("leftx") * ((parent.width - width) / 2 - 3)
                y: (parent.height - height) / 2 + root.axisValue("lefty") * ((parent.height - height) / 2 - 3)
                Behavior on x { NumberAnimation { duration: 50 } }
                Behavior on y { NumberAnimation { duration: 50 } }
            }
        }
        Item {
            x: 446; y: 196; width: 76; height: 76
            GamepadControl { diagram: root; elements: ["rightx", "righty"]; label: qsTr("Stick droit"); shape: "ring"; anchors.fill: parent }
            GamepadControl {
                diagram: root; elements: ["rightstick"]; label: qsTr("R")
                width: 34; height: 34
                x: (parent.width - width) / 2 + root.axisValue("rightx") * ((parent.width - width) / 2 - 3)
                y: (parent.height - height) / 2 + root.axisValue("righty") * ((parent.height - height) / 2 - 3)
                Behavior on x { NumberAnimation { duration: 50 } }
                Behavior on y { NumberAnimation { duration: 50 } }
            }
        }
    }
}
