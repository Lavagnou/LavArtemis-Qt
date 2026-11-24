import QtQuick
import QtQuick.Effects

// Mouse hover of a card or cover (prototype `ringMouse`): a soft shadow under the lifted card and
// a thin outline around it. Place it as a child of the card, anchors.fill: parent; the card itself
// does the 5 px lift (Translate) and the dimming (Theme.dim(hovered)).
Item {
    id: decor

    property bool active: false
    property real radius: Theme.r

    // The shadow goes under the card's background (z -1 in a Control) like LiveHalo
    z: -2

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 14
        anchors.bottomMargin: -10
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        radius: decor.radius
        color: "black"
        opacity: decor.active ? Theme.hoverShadowOpacity : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 150 } }
        layer.enabled: visible
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: 1.0
            blurMax: 32
        }
    }
}
