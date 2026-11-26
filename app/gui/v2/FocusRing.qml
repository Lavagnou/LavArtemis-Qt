import QtQuick

// Prototype `ring` (focus at the gamepad or the keyboard), drawn outside the item it decorates: a
// gap in the page colour, then the ring (Braise 3 + 3 px accent; Ludique 4 + 5 px navy in light,
// lime in dark). Affiche has no ring but `ring2`, a 4 px outline 4 px away; its yellow fill is the
// item's own background (Theme.ringFill). Sizes in prototype px times `k` (1.25 on a 1600 stage).
Item {
    id: ring

    property bool active: false
    property real radius: 0
    property real k: 1.25
    // Affiche covers and the launch button: outline only (ring2), never the yellow fill
    property bool outlineOnly: false

    anchors.fill: parent
    visible: active
    z: 10

    readonly property real gap: Theme.ringGap * k
    readonly property real w: Theme.ringWidth * k

    Rectangle {
        visible: !Theme.affiche
        anchors.fill: parent
        anchors.margins: -(ring.gap + ring.w)
        radius: ring.radius > 0 ? ring.radius + ring.gap + ring.w : 0
        color: "transparent"
        border.width: ring.w
        border.color: Theme.ringColor
    }
    Rectangle {
        visible: !Theme.affiche
        anchors.fill: parent
        anchors.margins: -ring.gap
        radius: ring.radius > 0 ? ring.radius + ring.gap : 0
        color: "transparent"
        border.width: ring.gap
        border.color: Theme.bg
    }
    Rectangle {
        visible: Theme.affiche
        anchors.fill: parent
        anchors.margins: -8 * ring.k
        color: "transparent"
        border.width: 4 * ring.k
        border.color: Theme.ring2Color
    }
}
