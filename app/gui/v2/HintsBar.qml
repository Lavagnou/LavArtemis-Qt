import QtQuick

// The prototype's hints bar (gamepad and keyboard only; nothing for the mouse or touch). `hints` is
// written for the gamepad, [{k: "A", l: "Lancer", act: function}], the keyboard version is derived
// as in the prototype (A -> Entrée, B -> Échap, Y -> Menu, LB RB -> Q E, Start -> Échap, "maintenir
// A" -> "maintenir Entrée"). Each hint is clickable. Braise: bottom right, round keycaps; Affiche:
// bottom left, black blocks with the key in yellow; Ludique: bottom right, a white capsule.
// Prototype px x k (1.25 on a 1600 stage).
Item {
    id: bar

    property real k: 1.25
    property var hints: []

    readonly property var keyNames: ({"A": qsTr("Entrée"), "B": qsTr("Échap"), "Y": qsTr("Menu"), "LB RB": "Q E", "Start": qsTr("Échap")})
    readonly property var shownHints: {
        if (Theme.input === "pad") return hints
        if (Theme.input !== "keys") return []
        var out = []
        for (var i = 0; i < hints.length; i++) {
            var h = hints[i]
            out.push({k: keyNames[h.k] || h.k, l: String(h.l).replace("maintenir A", qsTr("maintenir Entrée")), act: h.act})
        }
        return out
    }

    visible: shownHints.length > 0
    z: 90
    anchors.bottom: parent.bottom
    anchors.bottomMargin: (Theme.ludique ? 20 : 16) * k
    // x, not anchors: switching left / right anchors with the theme left both set
    x: Theme.affiche ? 32 * k : parent.width - width - (Theme.ludique ? 48 : 40) * k
    width: Theme.ludique ? capsule.width : row.width
    height: Theme.ludique ? capsule.height : row.height

    Rectangle {
        id: capsule
        visible: Theme.ludique
        width: row.width + 16 * bar.k
        height: 44 * bar.k
        radius: height / 2
        color: Theme.dark ? "#221C3D" : "#FFFFFF"
    }

    Row {
        id: row
        x: Theme.ludique ? 8 * bar.k : 0
        anchors.verticalCenter: Theme.ludique ? capsule.verticalCenter : undefined
        spacing: (Theme.braise ? 22 : (Theme.affiche ? 6 : 4)) * bar.k

        Repeater {
            model: bar.shownHints
            delegate: Rectangle {
                id: hint
                required property var modelData
                height: (Theme.affiche ? 34 : 36) * bar.k
                width: content.width + (Theme.affiche ? 24 : (Theme.ludique ? 14 : 0)) * bar.k
                radius: Theme.ludique ? height / 2 : 0
                color: Theme.affiche ? (Theme.dark ? "#EDEDE8" : "#111111") : "transparent"

                Row {
                    id: content
                    x: (Theme.affiche ? 12 : (Theme.ludique ? 4 : 0)) * bar.k
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8 * bar.k

                    Rectangle {
                        visible: !Theme.affiche
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(28 * bar.k, cap.implicitWidth + 16 * bar.k)
                        height: 28 * bar.k
                        radius: height / 2
                        color: Theme.ludique ? (Theme.dark ? "#F3EEFF" : "#1B1530") : (Theme.dark ? "#F3EEE8" : "#1E1A16")
                        Text {
                            id: cap
                            anchors.centerIn: parent
                            text: hint.modelData.k
                            color: Theme.ludique ? (Theme.dark ? "#3F5A00" : "#C6F432") : (Theme.dark ? "#0E0D0C" : "#F6F1EA")
                            font.family: Theme.textFont
                            font.pixelSize: 12 * bar.k
                            font.weight: Theme.ludique ? Font.ExtraBold : Font.Bold
                        }
                    }
                    Text {
                        visible: Theme.affiche
                        anchors.verticalCenter: parent.verticalCenter
                        text: hint.modelData.k
                        color: Theme.dark ? "#7A6000" : "#FFD60A"
                        font.family: Theme.textFont
                        font.pixelSize: 13 * bar.k
                        font.weight: Font.Black
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: hint.modelData.l
                        color: Theme.affiche ? (Theme.dark ? "#141414" : "#ECECE8")
                             : (Theme.ludique ? (Theme.dark ? "#F3EEFF" : "#1B1530") : Theme.text2)
                        font.family: Theme.textFont
                        font.pixelSize: (Theme.affiche ? 13 : 14) * bar.k
                        font.weight: Theme.affiche ? Font.DemiBold : (Theme.ludique ? Font.Bold : Font.Normal)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (hint.modelData.act) hint.modelData.act()
                }
            }
        }
    }
}
