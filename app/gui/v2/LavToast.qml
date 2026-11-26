import QtQuick
import QtQuick.Effects

// Toast of the prototype: centred at the top, 2.4 s, one at a time (a new one replaces the old).
// Braise: top 24, h40, pill, page text colour on the inverse. Affiche: top 0, yellow, black 3 px
// border without its top side, 15/800. Ludique: top 28, h48, lime, 15/800, soft shadow (light).
// Prototype px x k (1.25 on a 1600 stage).
Item {
    id: toast

    property real k: 1.25
    property string text: ""
    // What the last toast said, for the test channel (state.toasts)
    property var shown: []

    function show(message) {
        text = message
        shown.push(message)
        box.opacity = 1
        timer.restart()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    y: (Theme.affiche ? 0 : (Theme.ludique ? 28 : 24)) * k
    width: box.width
    height: box.height
    z: 100

    RectangularShadow {
        anchors.fill: box
        visible: Theme.ludique && !Theme.dark && box.opacity > 0
        opacity: box.opacity
        offset.y: 8 * toast.k
        blur: 24 * toast.k
        radius: box.radius
        color: Qt.rgba(27/255, 21/255, 48/255, 0.2)
    }

    Rectangle {
        id: box
        width: label.implicitWidth + 2 * (Theme.ludique ? 22 : 18) * toast.k
        height: (Theme.ludique ? 48 : 40) * toast.k
        radius: Theme.affiche ? 0 : height / 2
        color: Theme.affiche ? "#FFD60A" : (Theme.ludique ? "#C6F432" : Theme.inverse)
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 150 } }

        // Affiche: 3 px black border on the sides and bottom only
        Rectangle { visible: Theme.affiche; width: 3 * toast.k; height: parent.height; color: "#111111" }
        Rectangle { visible: Theme.affiche; x: parent.width - width; width: 3 * toast.k; height: parent.height; color: "#111111" }
        Rectangle { visible: Theme.affiche; y: parent.height - height; width: parent.width; height: 3 * toast.k; color: "#111111" }

        Text {
            id: label
            anchors.centerIn: parent
            text: toast.text
            color: Theme.affiche ? "#111111" : (Theme.ludique ? "#1B1530" : Theme.inverseInk)
            font.family: Theme.textFont
            font.pixelSize: (Theme.braise ? 14 : 15) * toast.k
            font.weight: Theme.braise ? Font.DemiBold : Font.ExtraBold
        }
    }

    Timer { id: timer; interval: 2400; onTriggered: box.opacity = 0 }
}
