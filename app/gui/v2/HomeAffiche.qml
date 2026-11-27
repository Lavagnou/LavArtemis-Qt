import QtQuick

// Home, Affiche (prototype V8, Affiche light/dark home; design-port-spec §1.3): a poster, no
// picture behind. Header band under a 3 px rule, the Scène as a black band, the two app cards as
// big numbered blocks with their art in a column, then the recent covers and « Tout voir ». Dark
// mode inverts the ink and the paper. Prototype px x k (1.25).
Item {
    id: lay

    required property var home
    readonly property real k: home.k
    readonly property bool dark: Theme.dark
    readonly property color ink: dark ? "#EDEDE8" : "#111111"
    readonly property color paper: dark ? "#141414" : "#ECECE8"
    readonly property color muted: dark ? "#A3A39C" : "#555550"
    // yellow text on the ink bands (dark: those bands are light, the yellow goes dark)
    readonly property color yellowOnInk: dark ? "#7A6000" : "#FFD60A"
    readonly property int rule: Math.round(3 * k)

    Rectangle { anchors.fill: parent; color: lay.paper }

    // ---- header ------------------------------------------------------------------------------------
    Item {
        id: header
        width: parent.width
        height: 60 * lay.k
        Row {
            id: brand
            x: 32 * lay.k
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10 * lay.k
            ThemeLogo { size: 30 * lay.k; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: "LavArtemis"
                color: lay.ink
                font.family: "Space Grotesk"
                font.weight: Font.Bold
                font.pixelSize: 18 * lay.k
                anchors.verticalCenter: parent.verticalCenter
            }
            UpdateButton { k: lay.k; anchors.verticalCenter: parent.verticalCenter }
        }
        readonly property real leftW: Math.max(260 * lay.k, brand.width)
        readonly property real rightW: Math.max(260 * lay.k, buttons.width)
        Row {
            id: switcher
            visible: lay.home.host !== null
            spacing: 14 * lay.k
            anchors.verticalCenter: parent.verticalCenter
            x: 32 * lay.k + header.leftW + (parent.width - 64 * lay.k - header.leftW - header.rightW - width) / 2
            SwitchButton { text: lay.home.lbLabel; onClicked: lay.home.shoulder(-1) }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: lay.home.host ? lay.home.host.name : ""
                color: lay.ink
                font.family: "Archivo"
                font.weight: Font.Black
                font.pixelSize: 20 * lay.k
                font.letterSpacing: -0.02 * 20 * lay.k
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: lay.home.hostStatus
                color: lay.muted
                font.family: "Archivo"
                font.weight: Font.DemiBold
                font.pixelSize: 14 * lay.k
            }
            SwitchButton { text: lay.home.rbLabel; onClicked: lay.home.shoulder(1) }
        }
        HomeHeaderButtons {
            id: buttons
            home: lay.home
            k: lay.k
            anchors.right: parent.right
            anchors.rightMargin: 32 * lay.k
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: lay.rule; color: lay.ink }
    }

    component SwitchButton: Rectangle {
        id: sb
        property string text: ""
        signal clicked()
        opacity: lay.home.hosts.length >= 2 ? 1 : 0
        height: sbText.implicitHeight + 10 * lay.k
        width: sbText.implicitWidth + 20 * lay.k
        color: "transparent"
        border.width: 2 * lay.k
        border.color: lay.ink
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        Text { id: sbText; anchors.centerIn: parent; text: sb.text; color: lay.ink; font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 12 * lay.k }
        MouseArea { anchors.fill: parent; enabled: sb.opacity > 0; cursorShape: Qt.PointingHandCursor; onClicked: sb.clicked() }
    }

    // A window lower than the prototype's scene (1000 x 600 is a stage 750 px high, the scene 900; with the Scène the
    // column is 663 px of the prototype): the Scène and the cards lose up to 16 px each, then the covers up to 60, so
    // that the recents and their focus ring stay inside the Stage, which clips (user's rule: a ring is never cut)
    readonly property real deficit: Math.max(0, (60 + (home.stageOn ? 90 : 0) + 270 + 26 + 14 + 198) * k
                                                + sectionText.implicitHeight - height) / k
    function take(before, slack) { return Math.min(Math.max(0, deficit - before), slack) }
    readonly property real cutStage: home.stageOn ? take(0, 16) : 0
    readonly property real cutCards: take(cutStage, 16)
    readonly property real coverH: (198 - take(cutStage + cutCards, 60)) * k

    Column {
        y: header.height
        width: parent.width

        // ---- Scène: the black band ---------------------------------------------------------------
        Rectangle {
            visible: lay.home.stageOn
            width: parent.width
            height: (90 - lay.cutStage) * lay.k
            color: lay.ink
            Column {
                x: 32 * lay.k
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 64 * lay.k - stageButtons.width - 24 * lay.k
                spacing: 4 * lay.k
                Row {
                    spacing: 8 * lay.k
                    LiveDot { k: lay.k; color: lay.yellowOnInk; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: qsTr("En cours — %1 — %2 — %3").arg(lay.home.host ? lay.home.host.name : "").arg(lay.home.stageTime).arg(lay.home.stageVia).toUpperCase()
                        color: lay.yellowOnInk
                        font.family: "Archivo"
                        font.weight: Font.ExtraBold
                        font.pixelSize: 13 * lay.k
                    }
                }
                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    text: lay.home.stageTitle
                    color: lay.paper
                    font.family: "Archivo"
                    font.weight: Font.Black
                    font.pixelSize: 40 * lay.k
                    font.letterSpacing: -0.045 * 40 * lay.k
                    lineHeight: 0.95
                }
            }
            Row {
                id: stageButtons
                anchors.right: parent.right
                anchors.rightMargin: 32 * lay.k
                anchors.verticalCenter: parent.verticalCenter
                spacing: 24 * lay.k
                HomeTile {
                    id: resumeTile
                    home: lay.home
                    fid: "resume"
                    k: lay.k
                    holdable: false
                    width: resumeText.implicitWidth + 48 * lay.k
                    height: 54 * lay.k
                    Rectangle { anchors.fill: parent; color: resumeTile.ringFill ? Theme.ringFillColor : lay.paper }
                    Text {
                        id: resumeText
                        anchors.centerIn: parent
                        text: lay.home.resumeLabel.toUpperCase() + " →"
                        color: resumeTile.ringFill ? Theme.ringFillFg : lay.ink
                        font.family: "Archivo"; font.weight: Font.Black; font.pixelSize: 20 * lay.k
                    }
                }
                HomeTile {
                    id: quitTile
                    home: lay.home
                    fid: "squit"
                    k: lay.k
                    holdable: false
                    width: quitText.implicitWidth + 46 * lay.k
                    height: 54 * lay.k
                    Rectangle {
                        anchors.fill: parent
                        color: quitTile.ringFill ? Theme.ringFillColor : "transparent"
                        border.width: lay.rule
                        border.color: quitTile.ringFill ? Theme.ringFillFg : lay.paper
                    }
                    Text {
                        id: quitText
                        anchors.centerIn: parent
                        text: qsTr("QUITTER")
                        color: quitTile.ringFill ? Theme.ringFillFg : lay.paper
                        font.family: "Archivo"; font.weight: Font.Black; font.pixelSize: 20 * lay.k
                    }
                }
            }
        }

        // ---- the two app cards ----------------------------------------------------------------------
        Item {
            visible: lay.home.homeCard === null
            width: parent.width
            height: (270 - lay.cutCards) * lay.k
            Row {
                anchors.fill: parent
                Repeater {
                    model: lay.home.cards
                    delegate: HomeTile {
                        id: card
                        required property var modelData
                        home: lay.home
                        fid: modelData.id
                        k: lay.k
                        width: parent.width / 2
                        height: parent.height
                        visible: modelData.present
                        dimmed: lay.home.sessionRunning && !modelData.running
                        glow: modelData.running
                        glowKind: "app"
                        moreSize: 38; moreRight: 14; moreTop: 14; moreRadius: 0; moreDots: 16
                        moreBg: lay.ink
                        moreFg: lay.paper
                        readonly property color cardInk: ringFill ? Theme.ringFillFg : lay.ink

                        Rectangle { anchors.fill: parent; color: card.ringFill ? Theme.ringFillColor : lay.paper }
                        Rectangle { anchors.right: parent.right; width: lay.rule; height: parent.height; color: lay.ink }
                        Item {
                            x: 26 * lay.k
                            y: 22 * lay.k
                            width: parent.width - 220 * lay.k - lay.rule * 2 - 52 * lay.k
                            height: parent.height - 44 * lay.k
                            Text {
                                text: qsTr("%1 — APPLICATION").arg(card.modelData.num)
                                color: card.cardInk
                                font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 14 * lay.k
                            }
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                spacing: 10 * lay.k
                                Text {
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    text: card.modelData.title
                                    color: card.cardInk
                                    font.family: "Archivo"; font.weight: Font.Black; font.pixelSize: 52 * lay.k
                                    font.letterSpacing: -0.045 * 52 * lay.k
                                    lineHeight: 0.9
                                }
                                Rectangle {
                                    visible: card.modelData.running
                                    width: Math.min(parent.width, runRow.width + 18 * lay.k)
                                    height: runRow.height + 10 * lay.k
                                    color: lay.ink
                                    Row {
                                        id: runRow
                                        x: 9 * lay.k
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 7 * lay.k
                                        LiveDot { k: lay.k; color: lay.dark ? "#111111" : "#FFD60A"; anchors.verticalCenter: parent.verticalCenter }
                                        Text {
                                            width: Math.min(implicitWidth, card.width - 330 * lay.k)
                                            wrapMode: Text.WordWrap
                                            text: qsTr("EN COURS — %1").arg(card.modelData.run)
                                            color: lay.dark ? "#111111" : "#FFD60A"
                                            font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 13 * lay.k
                                        }
                                    }
                                }
                                Text {
                                    visible: !card.modelData.running
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    text: card.modelData.sub
                                    color: card.cardInk
                                    lineHeight: 1.35
                                    font.family: "Archivo"; font.pixelSize: 15 * lay.k
                                }
                            }
                            Text {
                                anchors.bottom: parent.bottom
                                text: card.modelData.action + " →"
                                color: card.cardInk
                                font.family: "Archivo"; font.weight: Font.Black; font.pixelSize: 20 * lay.k
                            }
                        }
                        Rectangle {
                            // the art column, 220 wide, after a 3 px rule
                            anchors.right: parent.right
                            anchors.rightMargin: lay.rule
                            width: 220 * lay.k + lay.rule
                            height: parent.height
                            color: lay.ink
                            clip: true
                            Image {
                                x: lay.rule
                                width: parent.width - lay.rule
                                height: parent.height
                                source: card.modelData.art
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }
                        // lavbar: a yellow bar inside the bottom of the running card, 12 <-> 4 px
                        Rectangle {
                            id: bar
                            visible: card.modelData.running
                            anchors.bottom: parent.bottom
                            width: parent.width - 220 * lay.k - 2 * lay.rule
                            property real t: 0
                            height: (12 - 8 * t) * lay.k
                            color: "#FFD60A"
                            SequentialAnimation on t {
                                running: bar.visible
                                loops: Animation.Infinite
                                NumberAnimation { from: 0; to: 1; duration: 800; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 1; to: 0; duration: 800; easing.type: Easing.InOutQuad }
                            }
                        }
                    }
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: lay.rule; color: lay.ink }
        }

        // ---- section row --------------------------------------------------------------------------
        Item {
            visible: lay.home.homeCard === null && !lay.home.noGames
            width: parent.width
            height: (14 + 12) * lay.k + sectionText.implicitHeight
            Row {
                x: 32 * lay.k
                y: 14 * lay.k
                spacing: 18 * lay.k
                Text {
                    id: sectionText
                    text: qsTr("BIBLIOTHÈQUE PLAYNITE · RÉCENTS")
                    color: lay.ink
                    font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 15 * lay.k
                }
                Text {
                    text: lay.home.heroTitle
                    color: lay.muted
                    font.family: "Archivo"; font.weight: Font.Medium; font.pixelSize: 15 * lay.k
                }
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 32 * lay.k
                y: 14 * lay.k + (sectionText.implicitHeight - implicitHeight)
                text: qsTr("Tout voir →")
                color: lay.muted
                font.family: "Archivo"; font.weight: Font.Bold; font.pixelSize: 14 * lay.k
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: lay.home.act("lib") }
            }
        }

        // ---- recents + library tile -----------------------------------------------------------------
        Row {
            visible: lay.home.homeCard === null && !lay.home.noGames
            x: 32 * lay.k
            spacing: 18 * lay.k
            Repeater {
                model: lay.home.recents
                delegate: HomeTile {
                    id: cover
                    required property var modelData
                    required property int index
                    home: lay.home
                    fid: "r" + index
                    k: lay.k
                    outlineOnly: true
                    width: 132 * lay.k
                    height: lay.coverH
                    dimmed: lay.home.sessionRunning && !lay.home.isLive(modelData)
                    glow: lay.home.isLive(modelData)
                    moreSize: 32; moreRight: 0; moreTop: 0; moreRadius: 0; moreDots: 16
                    moreBg: lay.ink
                    moreFg: lay.paper
                    CoverFace { anchors.fill: parent; source: cover.modelData.cover; name: cover.modelData.name; bg: lay.ink; k: lay.k }
                }
            }
            HomeTile {
                id: libTile
                home: lay.home
                fid: "lib"
                k: lay.k
                holdable: false
                width: 200 * lay.k
                height: lay.coverH
                dimmed: lay.home.sessionRunning
                Rectangle {
                    anchors.fill: parent
                    color: libTile.ringFill ? Theme.ringFillColor : lay.paper
                    border.width: lay.rule
                    border.color: lay.ink
                }
                Text {
                    x: 16 * lay.k + lay.rule; y: 16 * lay.k + lay.rule
                    text: lay.home.libTotal.toUpperCase()
                    color: libTile.ringFill ? Theme.ringFillFg : lay.ink
                    font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 13 * lay.k
                }
                Text {
                    x: 16 * lay.k + lay.rule
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 16 * lay.k + lay.rule
                    width: parent.width - 32 * lay.k - 2 * lay.rule
                    wrapMode: Text.WordWrap
                    text: qsTr("Tout voir →")
                    color: libTile.ringFill ? Theme.ringFillFg : lay.ink
                    font.family: "Archivo"; font.weight: Font.Black; font.pixelSize: 34 * lay.k
                    font.letterSpacing: -0.03 * 34 * lay.k
                    lineHeight: 0.95
                }
            }
        }
        Item { visible: lay.home.homeCard === null && lay.home.noGames; width: 1; height: 18 * lay.k }
        NoGamesCard {
            visible: lay.home.homeCard === null && lay.home.noGames
            x: 32 * lay.k
            k: lay.k
            width: parent.width - 64 * lay.k
            height: 198 * lay.k
        }
    }

    // ---- offline / first run / searching / loading: one big block ---------------------------------------
    HomeTile {
        id: wake
        visible: lay.home.homeCard !== null
        home: lay.home
        fid: "wake"
        k: lay.k
        holdable: false
        x: 32 * lay.k
        y: 100 * lay.k
        width: parent.width - 64 * lay.k
        height: 260 * lay.k
        readonly property color wakeInk: wake.ringFill ? Theme.ringFillFg : lay.ink
        Rectangle {
            anchors.fill: parent
            color: wake.ringFill ? Theme.ringFillColor : lay.paper
            border.width: lay.rule
            border.color: lay.ink
        }
        Text {
            x: 32 * lay.k; y: 28 * lay.k
            text: lay.home.homeCard ? lay.home.homeCard.title : ""
            color: wake.wakeInk
            font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 15 * lay.k
        }
        Text {
            x: 32 * lay.k
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 64 * lay.k
            elide: Text.ElideRight
            text: lay.home.homeCard ? lay.home.homeCard.action : ""
            color: wake.wakeInk
            font.family: "Archivo"; font.weight: Font.Black; font.pixelSize: 88 * lay.k
            font.letterSpacing: -0.05 * 88 * lay.k
            lineHeight: 0.9
        }
        Text {
            x: 32 * lay.k
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 28 * lay.k
            text: lay.home.homeCard ? lay.home.homeCard.text : ""
            color: wake.wakeInk
            font.family: "Archivo"; font.pixelSize: 15 * lay.k
        }
    }
}
