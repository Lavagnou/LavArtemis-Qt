import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Half-width card of the home: Bureau or Playnite Fullscreen. Its own cover on the right (the
// user's cover from the host, never a game image), name and action on the left. While it runs:
// "EN COURS" badge, glow; while something else runs: dimmed ("Projecteur").
AbstractButton {
    id: card

    property string title: ""
    property string subtitle: ""
    property url cover: ""
    property bool running: false
    property bool dimmed: false
    property string liveLine: ""      // "EN COURS · Red Dead Redemption 2"

    signal menuRequested(real x, real y)

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    implicitHeight: 212

    LiveHalo {
        anchors.fill: parent
        active: card.running
        radius: Theme.r
    }

    // Dimmed while something else runs: the whole face, cover and text (Theme.dim)
    readonly property var dimStyle: Theme.dim(card.activeFocus && Theme.showFocus)

    background: Rectangle {
        id: bgRect
        radius: Theme.r
        color: card.running ? Qt.tint(Theme.surface, Qt.alpha(Theme.live, Theme.dark ? 0.10 : 0.14)) : Theme.surface
        border.width: Theme.border
        border.color: Theme.borderColor
        clip: true
        opacity: card.dimmed ? card.dimStyle.opacity : 1.0
        Behavior on opacity { NumberAnimation { duration: Theme.animMs } }
        layer.enabled: card.dimmed
        // Rendered at the stage's scale: a layer is otherwise drawn at the unscaled size and blurs
        layer.textureSize: Qt.size(width * Theme.stageScale * Screen.devicePixelRatio,
                                   height * Theme.stageScale * Screen.devicePixelRatio)
        layer.smooth: true
        layer.effect: MultiEffect { saturation: -card.dimStyle.gray }

        Image {
            id: coverImage
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: height * 670 / 900
            source: card.cover
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }
        Rectangle {
            // separator line between text and cover
            anchors.right: coverImage.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Theme.border
            color: Theme.borderColor
        }

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 32
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 212 * 670 / 900 + 24
            spacing: 14

            Rectangle {
                visible: card.running && card.liveLine.length > 0
                width: liveText.implicitWidth + 34
                height: 32
                radius: Theme.id === "affiche" ? 0 : 16
                color: Theme.live
                Row {
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.onLive; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        id: liveText
                        text: card.liveLine
                        color: Theme.onLive
                        font.family: Theme.textFont
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                }
            }
            Text {
                visible: !card.running
                width: parent.width
                text: card.subtitle
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 17
                elide: Text.ElideRight
            }
            // A narrow card (small window) wraps the title on two lines instead of cutting it
            // ("Playnite / Fullscreen"); only a word longer than the card is still elided.
            Text {
                width: parent.width
                text: Theme.displayText(card.title)
                color: Theme.fg
                font.family: Theme.displayFont
                font.pixelSize: 42
                font.weight: Theme.displayWeight
                font.letterSpacing: Theme.tracking * 42
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                lineHeight: 0.95
                elide: Text.ElideRight
            }
            Row {
                spacing: 10
                LavIcon { name: "play"; size: 12; color: card.running ? Theme.accent : Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: card.running ? qsTr("Rejoindre") : qsTr("Lancer")
                    color: card.running ? Theme.accent : Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 19
                    font.weight: Font.Bold
                }
            }
        }

        Rectangle {
            // brightness() of the dim: black over the face, multiplies its colours
            anchors.fill: parent
            radius: parent.radius
            color: "black"
            opacity: card.dimmed ? 1.0 - card.dimStyle.bright : 0.0
            visible: opacity > 0
        }
    }

    transform: Translate { y: card.hovered && Theme.input === "mouse" && !card.dimmed ? -5 : 0 }

    contentItem: Item {
        // "⋯": options. Mouse: only on hover; touch: always; pad/keys: Y / Menu instead.
        AbstractButton {
            id: moreButton
            anchors.right: parent.right
            anchors.rightMargin: 212 * 670 / 900 + 14
            anchors.top: parent.top
            anchors.topMargin: 14
            width: 40; height: 40
            visible: (card.hovered && Theme.input === "mouse") || Theme.input === "touch"
            focusPolicy: Qt.NoFocus
            background: Rectangle { radius: 20; color: Theme.surface2; opacity: 0.92 }
            contentItem: LavIcon { name: "dots"; color: Theme.fg; size: 20; anchors.centerIn: parent }
            onClicked: {
                var p = moreButton.mapToItem(card, 0, moreButton.height)
                card.menuRequested(p.x, p.y)
            }
        }
    }

    // Focus ring for gamepad and keyboard only.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -6
        radius: Theme.r + 6
        color: "transparent"
        border.width: 3
        border.color: Theme.accent
        visible: card.activeFocus && Theme.showFocus
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => {
            Theme.input = "mouse"
            card.menuRequested(eventPoint.position.x, eventPoint.position.y)
        }
    }
    TapHandler {
        acceptedDevices: PointerDevice.TouchScreen
        onLongPressed: {
            Theme.input = "touch"
            card.menuRequested(card.width / 2, card.height / 2)
        }
    }
}
