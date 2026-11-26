import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

// The prototype's segue (screen "segue", planches V8-24 connexion, V8-71 reprise, V8-25 fermeture):
// the game's background at 32 % (its cover blurred when it has none) under a vignette, then the
// cover, « ● LANCEMENT SUR ATLAS-PC… », the title, a sliding bar and the tip. Used by StreamSegue
// (start, resume, join) and QuitSegue (quit). Prototype px x k (1.25 on the 1600 stage).
Item {
    id: segue

    property string line: ""
    property string title: ""
    property string tip: ""
    property url cover: ""
    property url background: ""
    readonly property real k: 1.25

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Stage {
        id: stage
        anchors.fill: parent
        // the stage's own scale is k (1600 design px); the page's sizes are prototype px x segue.k
        readonly property real w: width / stage.k
        readonly property real h: height / stage.k

        // ---- background: picture at .32, blurred cover (BLUR) when there is no background --------
        Item {
            width: stage.w
            height: stage.h
            clip: true
            visible: pic.source.toString().length > 0
            opacity: 0.32
            Image {
                id: pic
                anchors.fill: parent
                source: segue.background.toString().length > 0 ? segue.background : segue.cover
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: !Theme.effects
            }
            MultiEffect {
                visible: Theme.effects
                anchors.fill: parent
                source: pic
                readonly property bool blurred: segue.background.toString().length === 0
                blurEnabled: blurred
                blurMax: 64
                blur: blurred ? 1.0 : 0
                saturation: blurred ? 0.2 : 0
                scale: blurred ? 1.25 : 1.0
            }
        }
        // radial-gradient(ellipse at center, transparent 0%, bg 78%): a circle on the half
        // diagonal of the width, squashed to the height (CSS farthest-corner ellipse)
        Shape {
            width: stage.w
            height: stage.w
            y: (stage.h - stage.w) / 2
            transform: Scale { origin.y: stage.w / 2; yScale: stage.h / stage.w }
            ShapePath {
                strokeWidth: -1
                fillGradient: RadialGradient {
                    centerX: stage.w / 2; centerY: stage.w / 2
                    centerRadius: stage.w / 2 * Math.SQRT2
                    focalX: centerX; focalY: centerY
                    GradientStop { position: 0.0; color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, 0) }
                    GradientStop { position: 0.78; color: Theme.bg }
                    GradientStop { position: 1.0; color: Theme.bg }
                }
                PathRectangle { x: 0; y: 0; width: stage.w; height: stage.w }
            }
        }

        // ---- the column, centred ------------------------------------------------------------------
        Column {
            id: col
            width: 820 * segue.k
            x: (stage.w - width) / 2
            y: (stage.h - height) / 2
            spacing: 16 * segue.k

            Item {
                visible: segue.cover.toString().length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                width: 132 * segue.k
                height: (198 + 8) * segue.k
                RectangularShadow {
                    visible: Theme.effects
                    anchors.fill: coverBox
                    offset.y: 18 * segue.k
                    blur: 40 * segue.k
                    radius: coverBox.radius
                    color: Qt.rgba(0, 0, 0, 0.45)
                }
                Rectangle {
                    id: coverBox
                    width: 132 * segue.k
                    height: 198 * segue.k
                    radius: Theme.rs * segue.k
                    color: Theme.surface2
                    layer.enabled: Theme.effects && radius > 0
                    layer.effect: MultiEffect { maskEnabled: true; maskSource: coverMask }
                    Image {
                        anchors.fill: parent
                        source: segue.cover
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }
                Rectangle {
                    id: coverMask
                    visible: false
                    layer.enabled: true
                    width: coverBox.width; height: coverBox.height
                    radius: coverBox.radius
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8 * segue.k
                LiveDot {
                    k: segue.k
                    color: Theme.ludique ? Theme.fg : Theme.accent
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: segue.line
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.weight: Font.Bold
                    font.pixelSize: 14 * segue.k
                    font.letterSpacing: 0.06 * 14 * segue.k
                    font.capitalization: Font.AllUppercase
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                text: segue.title
                color: Theme.fg
                font.family: Theme.displayFont
                font.weight: Theme.displayWeight
                font.pixelSize: 46 * segue.k
                font.letterSpacing: Theme.tracking * 46 * segue.k
                font.capitalization: Theme.upper ? Font.AllUppercase : Font.MixedCase
                lineHeight: 1.05
            }

            // The bar: 280 x 6 on « off », 40 % on « on » sliding through it (lavslide 1.3 s)
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 280 * segue.k
                height: (6 + 6) * segue.k
                Rectangle {
                    id: track
                    y: 6 * segue.k
                    width: parent.width
                    height: 6 * segue.k
                    radius: 3 * segue.k
                    color: Theme.off
                    clip: true
                    Rectangle {
                        id: slider
                        width: track.width * 0.4
                        height: track.height
                        radius: track.radius
                        color: Theme.on
                        NumberAnimation on x {
                            running: segue.visible
                            loops: Animation.Infinite
                            from: -1.1 * slider.width
                            to: 2.6 * slider.width
                            duration: 1300
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            }

            Text {
                visible: segue.tip.length > 0
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                topPadding: 10 * segue.k
                wrapMode: Text.Wrap
                text: segue.tip
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 14 * segue.k
            }
        }
    }
}
