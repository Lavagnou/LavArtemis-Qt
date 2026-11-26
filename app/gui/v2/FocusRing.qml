import QtQuick
import QtQuick.Window

// Prototype `ring` (focus at the gamepad or the keyboard), drawn outside the item it decorates: a
// gap in the page colour, then the ring (Braise 3 + 3 px accent; Ludique 4 + 5 px navy in light,
// lime in dark). Affiche has no ring but `ring2`, a 4 px outline 4 px away; its yellow fill is the
// item's own background (Theme.ringFill). Sizes in prototype px times `k` (1.25 on a 1600 stage).
// A ring is never cut (user's rule, 2026-11-26): the lists and panels that clip leave it room (scenario 19,
// test channel `focusrings`); Affiche's ring2 of an item that touches the window's edge (its home and details
// panels, its library rows) comes in on that side, so as to stay inside the window.
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

    // Affiche ring2: how far it may reach out on each side (8 px, less where the window ends)
    readonly property real out2: 8 * k
    property real roomL: out2
    property real roomT: out2
    property real roomR: out2
    property real roomB: out2
    function fitInWindow() {
        var win = ring.Window.window
        if (!active || !Theme.affiche || !win || !parent) {
            roomL = roomT = roomR = roomB = out2
            return
        }
        var p = parent.mapToItem(null, 0, 0)
        roomL = Math.min(out2, Math.max(0, p.x))
        roomT = Math.min(out2, Math.max(0, p.y))
        roomR = Math.min(out2, Math.max(0, win.width - p.x - parent.width))
        roomB = Math.min(out2, Math.max(0, win.height - p.y - parent.height))
    }
    onActiveChanged: fitInWindow()
    onWidthChanged: fitInWindow()
    onHeightChanged: fitInWindow()
    Component.onCompleted: fitInWindow()
    Connections {
        target: ring.Window.window
        ignoreUnknownSignals: true
        function onWidthChanged() { ring.fitInWindow() }
        function onHeightChanged() { ring.fitInWindow() }
    }
    Connections {
        target: Theme
        function onAfficheChanged() { ring.fitInWindow() }
    }

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
        anchors.leftMargin: -ring.roomL
        anchors.topMargin: -ring.roomT
        anchors.rightMargin: -ring.roomR
        anchors.bottomMargin: -ring.roomB
        color: "transparent"
        border.width: 4 * ring.k
        border.color: Theme.ring2Color
    }
}
