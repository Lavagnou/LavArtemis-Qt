import QtQuick 2.9

// The controller, laid out to scale. Clicking a control asks for it to be
// bound; the whole thing animates from live input, so a wrong or inverted axis
// shows up here rather than in a game.
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

    readonly property real designWidth: 1000
    readonly property real designHeight: 620
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
        return parts.length > 0 ? parts.join(" / ") : qsTr("nothing")
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

        Image {
            anchors.fill: parent
            source: "qrc:/res/gamepad_body.svg"
            fillMode: Image.PreserveAspectFit
            // Rasterize at the size it will actually occupy on screen. Without
            // this an SVG renders at its intrinsic size and is then scaled,
            // which looks soft at anything but a 1:1 window.
            sourceSize.width: root.designWidth * root.scaleFactor
            sourceSize.height: root.designHeight * root.scaleFactor
        }

        // ---- Triggers and shoulders ---------------------------------------

        GamepadControl {
            diagram: root; elements: ["lefttrigger"]; label: qsTr("LT")
            shape: "bar"; x: 240; y: 6; width: 110; height: 54
        }

        GamepadControl {
            diagram: root; elements: ["righttrigger"]; label: qsTr("RT")
            shape: "bar"; x: 650; y: 6; width: 110; height: 54
        }

        GamepadControl {
            diagram: root; elements: ["leftshoulder"]; label: qsTr("LB")
            shape: "pill"; x: 230; y: 68; width: 130; height: 34
        }

        GamepadControl {
            diagram: root; elements: ["rightshoulder"]; label: qsTr("RB")
            shape: "pill"; x: 640; y: 68; width: 130; height: 34
        }

        // ---- Centre cluster -----------------------------------------------

        GamepadControl {
            diagram: root; elements: ["guide"]; label: qsTr("Home")
            x: 475; y: 160; width: 50; height: 50
        }

        GamepadControl {
            diagram: root; elements: ["back"]; label: qsTr("Back")
            shape: "pill"; x: 396; y: 232; width: 58; height: 26
        }

        GamepadControl {
            diagram: root; elements: ["start"]; label: qsTr("Start")
            shape: "pill"; x: 546; y: 232; width: 58; height: 26
        }

        // ---- Face buttons -------------------------------------------------

        GamepadControl {
            diagram: root; elements: ["y"]; label: qsTr("Y")
            x: 678; y: 166; width: 54; height: 54
        }

        GamepadControl {
            diagram: root; elements: ["x"]; label: qsTr("X")
            x: 621; y: 223; width: 54; height: 54
        }

        GamepadControl {
            diagram: root; elements: ["b"]; label: qsTr("B")
            x: 735; y: 223; width: 54; height: 54
        }

        GamepadControl {
            diagram: root; elements: ["a"]; label: qsTr("A")
            x: 678; y: 280; width: 54; height: 54
        }

        // ---- D-pad --------------------------------------------------------

        Rectangle {
            x: 374; y: 360; width: 42; height: 42
            color: "#1f1f1f"
            border.color: "#404040"
            border.width: 2
            radius: 4
        }

        GamepadControl {
            diagram: root; elements: ["dpup"]; label: "▲"
            shape: "wedge"; x: 377; y: 316; width: 36; height: 42
        }

        GamepadControl {
            diagram: root; elements: ["dpdown"]; label: "▼"
            shape: "wedge"; x: 377; y: 404; width: 36; height: 42
        }

        GamepadControl {
            diagram: root; elements: ["dpleft"]; label: "◀"
            shape: "wedge"; x: 330; y: 363; width: 42; height: 36
        }

        GamepadControl {
            diagram: root; elements: ["dpright"]; label: "▶"
            shape: "wedge"; x: 418; y: 363; width: 42; height: 36
        }

        // ---- Sticks -------------------------------------------------------
        // The ring binds the axis pair and the disc binds the click. The disc
        // is also what moves, so it is target and readout at once -- which is
        // what a stick physically is, and it keeps the centre unambiguous.

        Item {
            x: 239; y: 194; width: 112; height: 112

            GamepadControl {
                diagram: root; elements: ["leftx", "lefty"]; label: qsTr("Left stick")
                shape: "ring"; anchors.fill: parent
            }

            GamepadControl {
                diagram: root; elements: ["leftstick"]; label: qsTr("L3")
                width: 52; height: 52
                x: (parent.width - width) / 2
                   + root.axisValue("leftx") * ((parent.width - width) / 2 - 4)
                y: (parent.height - height) / 2
                   + root.axisValue("lefty") * ((parent.height - height) / 2 - 4)

                Behavior on x { NumberAnimation { duration: 50 } }
                Behavior on y { NumberAnimation { duration: 50 } }
            }
        }

        Item {
            x: 549; y: 325; width: 112; height: 112

            GamepadControl {
                diagram: root; elements: ["rightx", "righty"]; label: qsTr("Right stick")
                shape: "ring"; anchors.fill: parent
            }

            GamepadControl {
                diagram: root; elements: ["rightstick"]; label: qsTr("R3")
                width: 52; height: 52
                x: (parent.width - width) / 2
                   + root.axisValue("rightx") * ((parent.width - width) / 2 - 4)
                y: (parent.height - height) / 2
                   + root.axisValue("righty") * ((parent.height - height) / 2 - 4)

                Behavior on x { NumberAnimation { duration: 50 } }
                Behavior on y { NumberAnimation { duration: 50 } }
            }
        }
    }
}
