import QtQuick
import QtQuick.Effects

// A focusable item of the prototype's grid screens (home cards, covers, « Tout voir », buttons of
// the Scène): what the prototype's item() gives every one of them.
//  - focus at the gamepad / keyboard: `ring` (FocusRing) and scale 1.04; Affiche: `ring2` only
//    (the yellow fill is drawn by the content, which reads `ringFill`);
//  - focus at the mouse (PC, it follows the pointer): `ringMouse`, lifted 5 px, soft shadow, thin
//    ring; Affiche has none and uses its ring instead;
//  - dimmed (another app runs): dimOff, dimFocus when focused (Theme.dim);
//  - glow (running): LiveHalo behind;
//  - click / Enter = activate, right click or "⋯" = options at the pointer, press 500 ms =
//    options as a centred dialog (the release then does not activate).
// Sizes: prototype px x k.
Item {
    id: tile

    property var home: null
    property string fid: ""
    property real k: 1.25
    property real radius: 0
    property bool dimmed: false
    property bool glow: false
    property string glowKind: "cover"     // "app" | "cover"
    property bool holdable: true
    property bool outlineOnly: false      // Affiche covers: ring2 only, never the yellow fill
    property bool ringShown: true         // false: the focus is the content's own (Affiche rows of the in-game menu: fill only)
    // "⋯" button: shown at the mouse on hover only (PC), always at the gamepad / keyboard, x1.3 at touch
    property bool hasMore: holdable
    property real moreSize: 38
    property real moreRight: 14
    property real moreTop: 14
    property color moreBg: Qt.rgba(0, 0, 0, 0.72)
    property color moreFg: "white"
    property real moreRadius: moreSize / 2
    property real moreDots: 18
    property bool moreShadow: false
    property color moreShadowColor: Qt.rgba(27/255, 21/255, 48/255, 0.25)
    default property alias content: face.data

    readonly property bool focused: home !== null && home.isFocused(fid)
    readonly property bool padRing: focused && Theme.showFocus
    readonly property bool mouseRing: focused && Theme.input === "mouse" && !Theme.affiche
    // Affiche: the yellow fill of the focused item (content reads it)
    readonly property bool ringFill: Theme.affiche && focused && (Theme.showFocus || Theme.input === "mouse") && !outlineOnly
    readonly property var dimStyle: Theme.dim(focused)

    transform: Translate {
        y: tile.mouseRing ? -5 * tile.k : 0
        Behavior on y { NumberAnimation { duration: 150 } }
    }
    scale: tile.padRing && !Theme.affiche ? Theme.focusScale : 1.0
    Behavior on scale { NumberAnimation { duration: 150 } }
    z: focused ? 2 : 0

    LiveHalo {
        anchors.fill: parent
        active: tile.glow
        radius: tile.radius
        kind: tile.glowKind
        k: tile.k
    }
    RectangularShadow {
        anchors.fill: parent
        visible: tile.mouseRing
        offset.y: 18 * tile.k
        blur: 36 * tile.k
        radius: tile.radius
        color: Theme.hoverShadow
        z: -1
    }

    Rectangle {
        // ringMouse: thin ring of the hovered item
        anchors.fill: parent
        anchors.margins: -Theme.hoverRingWidth * tile.k
        radius: tile.radius > 0 ? tile.radius + Theme.hoverRingWidth * tile.k : 0
        color: "transparent"
        border.width: Theme.hoverRingWidth * tile.k
        border.color: Theme.hoverRing
        visible: tile.mouseRing
    }
    FocusRing {
        active: tile.ringShown && (tile.padRing || (Theme.affiche && tile.focused && Theme.input === "mouse"))
        radius: tile.radius
        k: tile.k
    }

    MouseArea {
        id: hit
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        pressAndHoldInterval: 500
        property bool held: false
        // The content is inside the MouseArea: a click on a text of the tile reaches it (and the test
        // channel sees the tile, not something covering it)
        Item {
            id: face
            // the content can say `radius: parent.radius`
            readonly property real radius: tile.radius
            anchors.fill: parent
            opacity: tile.dimmed ? tile.dimStyle.opacity : 1.0
            Behavior on opacity { NumberAnimation { duration: 150 } }
            layer.enabled: tile.dimmed && Theme.effects
            layer.smooth: true
            layer.effect: MultiEffect { saturation: -tile.dimStyle.gray }
        }
        Rectangle {
            // brightness() of the dim: black over the face multiplies its colours
            anchors.fill: parent
            radius: tile.radius
            color: "black"
            opacity: tile.dimmed ? 1.0 - tile.dimStyle.bright : 0.0
            visible: opacity > 0
        }

        onEntered: if (tile.home) tile.home.hover(tile.fid)
        onPressed: (m) => {
            held = false
            if (m.button === Qt.RightButton && tile.home) {
                Theme.input = "mouse"
                tile.home.options(tile.fid, hit, m.x, m.y)
            }
        }
        onPressAndHold: (m) => {
            if (m.button === Qt.LeftButton && tile.holdable && tile.home) {
                held = true
                // a long press is a centred dialog, not the popover
                var was = Theme.input
                if (was === "mouse") Theme.input = "keys"
                tile.home.options(tile.fid, null, 0, 0)
                if (was === "mouse") Theme.input = "mouse"
            }
        }
        onClicked: (m) => {
            if (m.button !== Qt.LeftButton || held || !tile.home) return
            tile.home.focusId = tile.fid
            tile.home.act(tile.fid)
        }
    }

    Rectangle {
        id: more
        visible: tile.hasMore && Theme.input !== "none"
                 && (Theme.input !== "mouse" || hit.containsMouse || moreHit.containsMouse)
        width: tile.moreSize * tile.k
        height: width
        radius: tile.moreRadius * tile.k
        anchors.right: parent.right
        anchors.rightMargin: tile.moreRight * tile.k
        anchors.top: parent.top
        anchors.topMargin: tile.moreTop * tile.k
        color: tile.moreBg
        scale: Theme.input === "touch" ? 1.3 : 1.0
        transformOrigin: Item.TopRight
        layer.enabled: tile.moreShadow && Theme.effects
        layer.effect: MultiEffect { shadowEnabled: true; shadowColor: tile.moreShadowColor; shadowVerticalOffset: 2 * tile.k; shadowBlur: 0.25 }
        Row {
            anchors.centerIn: parent
            spacing: 3 * tile.k * tile.moreDots / 24
            Repeater {
                model: 3
                Rectangle { width: 4 * tile.k * tile.moreDots / 24; height: width; radius: width / 2; color: tile.moreFg }
            }
        }
        MouseArea {
            id: moreHit
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: (m) => {
                if (!tile.home) return
                var was = Theme.input
                if (was !== "mouse") Theme.input = "keys" // pad / touch: a centred dialog
                tile.home.options(tile.fid, was === "mouse" ? moreHit : null, m.x, m.y)
                if (was !== "mouse") Theme.input = was
            }
        }
    }
}
