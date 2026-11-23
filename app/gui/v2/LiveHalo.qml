import QtQuick
import QtQuick.Effects

// The pulsing glow behind the running app or game (ember for Braise, hard yellow shadow for
// Affiche, lime halo for Ludique). Place it under the card, same geometry.
Item {
    id: halo

    property bool active: false
    property real radius: Theme.r

    visible: active
    z: -1

    // Affiche: a hard offset block instead of a blur, blinking like a poster light.
    Rectangle {
        visible: Theme.id === "affiche"
        x: 10; y: 10
        width: parent.width; height: parent.height
        color: Theme.live
        SequentialAnimation on opacity {
            running: halo.active && Theme.id === "affiche"
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 0.55; duration: 800; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 0.55; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
        }
    }

    Rectangle {
        id: glow
        visible: Theme.id !== "affiche"
        anchors.fill: parent
        anchors.margins: -6
        radius: halo.radius + 6
        color: Theme.live
        layer.enabled: visible
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: 1.0
            blurMax: 48
        }
        SequentialAnimation on opacity {
            running: halo.active && Theme.id !== "affiche"
            loops: Animation.Infinite
            NumberAnimation { from: 0.35; to: 0.75; duration: 1300; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.75; to: 0.35; duration: 1300; easing.type: Easing.InOutSine }
        }
    }
}
