import QtQuick
import QtQuick.Effects

// Glow of the running app card / cover (prototype glowApp / glowCover), behind the item:
// Braise `lavember` 2.6 s, orange drop shadow 10 <-> 26 px, alpha .45 <-> .95; Ludique `lavplum`
// 2.4 s, purple #7B5CFF 6 <-> 20 px, .5 <-> 1 (both modes); Affiche `lavpop` 1.6 s, a hard yellow
// shadow offset 10 <-> 5 px (covers; app cards draw their `lavbar` themselves). Prototype px x k.
Item {
    id: halo

    property bool active: false
    property real radius: Theme.r
    property real k: 1.25
    // "app" or "cover": Affiche app cards have no halo (their bar is inside the card)
    property string kind: "cover"

    visible: active && !(Theme.affiche && kind === "app")
    z: -2

    property real t: 0
    SequentialAnimation on t {
        running: halo.visible
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: Theme.glowMs / 2; easing.type: Easing.InOutQuad }
        NumberAnimation { from: 1; to: 0; duration: Theme.glowMs / 2; easing.type: Easing.InOutQuad }
    }

    Rectangle {
        visible: Theme.affiche
        x: (10 - 5 * halo.t) * halo.k
        y: x
        width: parent.width; height: parent.height
        color: "#FFD60A"
    }

    Rectangle {
        id: source
        visible: !Theme.affiche && Theme.effects
        anchors.fill: parent
        radius: halo.radius
        color: Theme.glowColor
        layer.enabled: visible && Theme.effects
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.glowColor
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 0
            blurMax: 64
            shadowBlur: (Theme.ludique ? 6 + 14 * halo.t : 10 + 16 * halo.t) * halo.k / 64 * 2
            shadowOpacity: Theme.ludique ? 0.5 + 0.5 * halo.t : 0.45 + 0.5 * halo.t
            shadowScale: 1.0
        }
    }
}
