import QtQuick
import QtQuick.Effects

// A game's cover, cut to the tile's radius; the game's name when it has none (LavApollo 404, or
// no_app_image.png).
Item {
    id: face
    property url source: ""
    property string name: ""
    property real radius: 0
    property color bg: Theme.surface
    property real k: 1.25
    // The prototype's BLUR (blur 28 px, saturate 1.2): a cover standing in for a background
    property bool blurred: false

    readonly property bool hasCover: img.status === Image.Ready && String(source).indexOf("no_app_image") < 0

    Rectangle { anchors.fill: parent; radius: face.radius; color: face.bg }
    Image {
        id: img
        anchors.fill: parent
        source: face.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: face.hasCover
        layer.enabled: (face.radius > 0 || face.blurred) && visible && Theme.effects
        layer.effect: MultiEffect {
            maskEnabled: face.radius > 0
            maskSource: mask
            blurEnabled: face.blurred
            blurMax: 64
            blur: face.blurred ? 1.0 : 0
            saturation: face.blurred ? 0.2 : 0
        }
    }
    Rectangle { id: mask; anchors.fill: parent; radius: face.radius; visible: false; layer.enabled: true }
    Text {
        visible: !face.hasCover
        anchors.fill: parent
        anchors.margins: 10 * face.k
        text: face.name
        color: Theme.fg
        wrapMode: Text.WordWrap
        maximumLineCount: 4
        elide: Text.ElideRight
        verticalAlignment: Text.AlignBottom
        font.family: Theme.displayFont
        font.weight: Theme.displayWeight
        font.pixelSize: 16 * face.k
    }
}
