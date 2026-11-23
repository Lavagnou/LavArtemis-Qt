import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// A game of the Playnite library (or one of the user's other apps): portrait cover, name under
// it on hover/focus. Same running/dimmed/options behaviour as AppCard.
AbstractButton {
    id: tile

    property string title: ""
    property url cover: ""
    property bool running: false
    property bool dimmed: false

    signal menuRequested(real x, real y)

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    implicitWidth: 150
    implicitHeight: 225

    LiveHalo {
        anchors.fill: parent
        active: tile.running
        radius: Theme.rs
    }

    transform: Translate { y: tile.hovered && Theme.input === "mouse" && !tile.dimmed ? -5 : 0 }

    // Dimmed while another app runs: the whole face (cover, or the name when there is none)
    readonly property var dimStyle: Theme.dim(tile.activeFocus && Theme.showFocus)

    background: Rectangle {
        radius: Theme.rs
        color: Theme.surface2
        border.width: Theme.border
        border.color: Theme.borderColor
        clip: true
        opacity: tile.dimmed ? tile.dimStyle.opacity : 1.0
        Behavior on opacity { NumberAnimation { duration: Theme.animMs } }
        layer.enabled: tile.dimmed
        // Rendered at the stage's scale: a layer is otherwise drawn at the unscaled size and blurs
        layer.textureSize: Qt.size(width * Theme.stageScale * Screen.devicePixelRatio,
                                   height * Theme.stageScale * Screen.devicePixelRatio)
        layer.smooth: true
        layer.effect: MultiEffect { saturation: -tile.dimStyle.gray }

        Text {
            // Without a cover: the name, big, on the theme's second surface
            anchors.fill: parent
            anchors.margins: 14
            visible: coverImage.status !== Image.Ready
            text: Theme.displayText(tile.title)
            color: Theme.muted
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignBottom
            font.family: Theme.displayFont
            font.pixelSize: 20
            font.weight: Theme.displayWeight
        }

        Image {
            id: coverImage
            anchors.fill: parent
            anchors.margins: Theme.border
            // The client's grey placeholder (cover not fetched yet, or none on the host) is not a
            // cover: the name shows instead, as in the prototype
            source: tile.cover.toString() === "qrc:/res/no_app_image.png" ? "" : tile.cover
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: 300
            layer.enabled: Theme.rs > 0
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: mask
            }
        }
        Rectangle {
            id: mask
            anchors.fill: coverImage
            radius: Math.max(0, Theme.rs - Theme.border)
            visible: false
            layer.enabled: true
        }
        Rectangle {
            // brightness() of the dim: black over the face, multiplies its colours
            anchors.fill: parent
            radius: parent.radius
            color: "black"
            opacity: tile.dimmed ? 1.0 - tile.dimStyle.bright : 0.0
            visible: opacity > 0
        }
    }

    contentItem: Item {
        Rectangle {
            visible: tile.running
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            width: badgeText.implicitWidth + 20; height: 26
            radius: Theme.id === "affiche" ? 0 : 13
            color: Theme.live
            Text {
                id: badgeText
                anchors.centerIn: parent
                text: qsTr("EN COURS")
                color: Theme.onLive
                font.family: Theme.textFont
                font.pixelSize: 12
                font.weight: Font.Bold
            }
        }

        AbstractButton {
            id: moreButton
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            width: 36; height: 36
            visible: (tile.hovered && Theme.input === "mouse") || Theme.input === "touch"
            focusPolicy: Qt.NoFocus
            background: Rectangle { radius: 18; color: Theme.surface; opacity: 0.92 }
            contentItem: LavIcon { name: "dots"; color: Theme.fg; size: 18; anchors.centerIn: parent }
            onClicked: {
                var p = moreButton.mapToItem(tile, 0, moreButton.height)
                tile.menuRequested(p.x, p.y)
            }
        }
    }

    // Name under the cover, on hover or focus
    Text {
        anchors.top: parent.bottom
        anchors.topMargin: 8
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        visible: tile.hovered || tile.activeFocus
        text: tile.title
        color: Theme.fg
        elide: Text.ElideRight
        font.family: Theme.textFont
        font.pixelSize: 14
        font.weight: Font.DemiBold
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -6
        radius: Theme.rs + 6
        color: "transparent"
        border.width: 3
        border.color: Theme.accent
        visible: tile.activeFocus && Theme.showFocus
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => {
            Theme.input = "mouse"
            tile.menuRequested(eventPoint.position.x, eventPoint.position.y)
        }
    }
    TapHandler {
        acceptedDevices: PointerDevice.TouchScreen
        onLongPressed: {
            Theme.input = "touch"
            tile.menuRequested(tile.width / 2, tile.height / 2)
        }
    }
}
