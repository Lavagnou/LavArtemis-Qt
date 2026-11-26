import QtQuick
import QtQuick.Effects

// Home, Braise (prototype V8, Proto.dc.html Braise dark/light home; design-port-spec §1.3):
// full-bleed hero picture under a veil, the header, the hero title or the Scène, and at the bottom
// « Sur {PC} », the two app cards and the recent games. Prototype px x k (1.25).
Item {
    id: lay

    required property var home
    readonly property real k: home.k
    readonly property bool dark: Theme.dark
    // Braise literal colours (prototype markup: dark / light)
    readonly property color bgc: dark ? "#0E0D0C" : "#F6F1EA"
    readonly property color fgc: dark ? "#F3EEE8" : "#1E1A16"
    readonly property color surf: dark ? "#1A1816" : "#FFFFFF"
    readonly property color text2: dark ? "#E7E0D8" : "#3E3731"
    readonly property color text3: dark ? "#CFC6BC" : "#6B6158"
    readonly property color accentText: dark ? "#FF8A57" : "#C9480F"
    readonly property color chipBorder: dark ? "#5A524B" : "#B8AA9B"
    function veil(a) { return dark ? Qt.rgba(14/255, 13/255, 12/255, a) : Qt.rgba(246/255, 241/255, 234/255, a) }

    // ---- hero ---------------------------------------------------------------------------------
    Item {
        anchors.fill: parent
        clip: true
        Image {
            id: heroImage
            anchors.fill: parent
            source: lay.home.hero.src
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: !Theme.effects
        }
        MultiEffect {
            visible: Theme.effects
            // blur(28px) saturate(1.2) scale(1.25) when the picture is a cover, not a background
            anchors.fill: parent
            source: heroImage
            scale: lay.home.hero.blur ? 1.25 : 1.0
            blurEnabled: lay.home.hero.blur
            blurMax: 64
            blur: lay.home.hero.blur ? 28 * lay.k / 64 * 1.6 : 0
            saturation: lay.home.hero.blur ? 0.2 : 0
            opacity: heroImage.status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: lay.veil(0.25) }
                GradientStop { position: 0.38; color: lay.veil(0.6) }
                GradientStop { position: 0.74; color: lay.bgc }
                GradientStop { position: 1.0; color: lay.bgc }
            }
        }
    }

    // ---- header -------------------------------------------------------------------------------
    Item {
        id: header
        x: 40 * lay.k
        y: 24 * lay.k
        width: parent.width - 80 * lay.k
        height: 44 * lay.k

        Row {
            id: brand
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10 * lay.k
            ThemeLogo { size: 34 * lay.k; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: "LavArtemis"
                color: lay.fgc
                font.family: "Space Grotesk"
                font.weight: Font.Bold
                font.pixelSize: 22 * lay.k
                font.letterSpacing: -0.4 * lay.k
                anchors.verticalCenter: parent.verticalCenter
            }
            DevChip { k: lay.k; anchors.verticalCenter: parent.verticalCenter }
            UpdateButton { k: lay.k; anchors.verticalCenter: parent.verticalCenter }
        }
        readonly property real leftW: Math.max(300 * lay.k, brand.width)
        readonly property real rightW: Math.max(300 * lay.k, buttons.width)

        // Host switcher: dark translucent pill, LB / RB (Q / E, ‹ ›) around the name and status
        Rectangle {
            id: switcher
            visible: lay.home.host !== null
            height: 44 * lay.k
            width: swRow.width + 12 * lay.k
            radius: 22 * lay.k
            color: lay.veil(0.7)
            x: header.leftW + (parent.width - header.leftW - header.rightW - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            Row {
                id: swRow
                x: 6 * lay.k
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8 * lay.k
                SwitchButton { text: lay.home.lbLabel; onClicked: lay.home.shoulder(-1) }
                Column {
                    width: Math.max(190 * lay.k, implicitWidth)
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: lay.home.host ? lay.home.host.name : ""
                        color: lay.fgc
                        font.family: "Inter"
                        font.weight: Font.Bold
                        font.pixelSize: 15 * lay.k
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: lay.home.hostStatus
                        color: lay.text3
                        font.family: "Inter"
                        font.pixelSize: 12 * lay.k
                    }
                }
                SwitchButton { text: lay.home.rbLabel; onClicked: lay.home.shoulder(1) }
            }
        }
        HomeHeaderButtons {
            id: buttons
            home: lay.home
            k: lay.k
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    component SwitchButton: Rectangle {
        id: sb
        property string text: ""
        signal clicked()
        visible: lay.home.hosts.length >= 2
        height: 32 * lay.k
        width: sbText.implicitWidth + 20 * lay.k + 2 * lay.k
        radius: 16 * lay.k
        color: "transparent"
        border.width: lay.k
        border.color: lay.chipBorder
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        Text {
            id: sbText
            anchors.centerIn: parent
            text: sb.text
            color: lay.text3
            font.family: "Inter"
            font.weight: Font.Bold
            font.pixelSize: 12 * lay.k
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: sb.clicked() }
    }

    // ---- hero title (a recent game focused, no session) -------------------------------------------
    Column {
        visible: lay.home.focusedRecent !== null && !lay.home.stageOn
        x: 40 * lay.k
        y: 104 * lay.k
        width: 700 * lay.k
        spacing: 6 * lay.k
        Text {
            text: qsTr("Bibliothèque Playnite · %1").arg(lay.home.focusedRecent ? lay.home.hoursText(lay.home.focusedRecent) : "")
            color: lay.text2
            font.family: "Inter"
            font.weight: Font.DemiBold
            font.pixelSize: 14 * lay.k
        }
        Text {
            width: parent.width
            text: lay.home.focusedRecent ? lay.home.focusedRecent.name : ""
            color: lay.fgc
            elide: Text.ElideRight
            font.family: "Space Grotesk"
            font.weight: Font.Bold
            font.pixelSize: 46 * lay.k
            font.letterSpacing: -0.02 * 46 * lay.k
            lineHeight: 1.05
        }
    }

    // ---- Scène --------------------------------------------------------------------------------------
    Column {
        visible: lay.home.stageOn
        x: 40 * lay.k
        y: 84 * lay.k
        width: 760 * lay.k
        spacing: 6 * lay.k
        Row {
            spacing: 8 * lay.k
            LiveDot { k: lay.k; color: lay.accentText; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: qsTr("EN COURS SUR %1 · %2").arg(lay.home.host ? lay.home.host.name : "").arg(lay.home.stageTime)
                color: lay.accentText
                font.family: "Inter"
                font.weight: Font.Bold
                font.pixelSize: 13 * lay.k
                font.letterSpacing: 0.06 * 13 * lay.k
            }
        }
        Text {
            width: parent.width
            text: lay.home.stageTitle
            color: lay.fgc
            elide: Text.ElideRight
            font.family: "Space Grotesk"
            font.weight: Font.Bold
            font.pixelSize: 42 * lay.k
            font.letterSpacing: -0.02 * 42 * lay.k
            lineHeight: 1.02
        }
        Item { width: 1; height: 4 * lay.k }
        Row {
            spacing: 14 * lay.k
            HomeTile {
                home: lay.home
                fid: "resume"
                k: lay.k
                holdable: false
                radius: 21 * lay.k
                width: resumeRow.width + 40 * lay.k
                height: 42 * lay.k
                Rectangle { anchors.fill: parent; radius: parent.height / 2; color: "#FF6A2B" }
                Row {
                    id: resumeRow
                    anchors.centerIn: parent
                    spacing: 8 * lay.k
                    LavIcon { path: Theme.icons.play; filled: true; stroke: 0; size: 14 * lay.k; color: "#0E0D0C"; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: lay.home.resumeLabel; color: "#0E0D0C"; font.family: "Inter"; font.weight: Font.Bold; font.pixelSize: 16 * lay.k }
                }
            }
            HomeTile {
                home: lay.home
                fid: "squit"
                k: lay.k
                holdable: false
                radius: 21 * lay.k
                width: quitText.implicitWidth + 36 * lay.k
                height: 42 * lay.k
                Rectangle { anchors.fill: parent; radius: parent.height / 2; color: "transparent"; border.width: lay.k; border.color: lay.chipBorder }
                Text { id: quitText; anchors.centerIn: parent; text: qsTr("Quitter"); color: lay.fgc; font.family: "Inter"; font.weight: Font.Bold; font.pixelSize: 16 * lay.k }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 · ⋯ sur la carte pour relancer").arg(lay.home.stageVia)
                color: lay.text2
                font.family: "Inter"
                font.pixelSize: 14 * lay.k
            }
        }
    }

    // ---- bottom block: « Sur {PC} », cards (or the offline card), recents --------------------------------
    Column {
        id: bottom
        x: 40 * lay.k
        width: parent.width - 80 * lay.k
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 64 * lay.k
        spacing: 12 * lay.k

        Text {
            visible: text !== ""
            text: lay.home.homeCard && lay.home.homeCard.kind === "firstrun" ? "" : lay.home.hostLine
            color: lay.text2
            font.family: "Inter"
            font.weight: Font.DemiBold
            font.pixelSize: 14 * lay.k
        }

        // Offline, first run, searching, loading
        HomeTile {
            visible: lay.home.homeCard !== null
            home: lay.home
            fid: "wake"
            k: lay.k
            holdable: false
            radius: 22 * lay.k
            width: Math.min(680 * lay.k, Math.max(520 * lay.k, cardAction.implicitWidth + 56 * lay.k))
            height: 150 * lay.k
            Rectangle { anchors.fill: parent; radius: 22 * lay.k; color: lay.surf }
            Column {
                x: 28 * lay.k
                y: 24 * lay.k
                width: parent.width - 56 * lay.k
                height: parent.height - 48 * lay.k
                Text { text: lay.home.homeCard ? lay.home.homeCard.title : ""; color: lay.text3; font.family: "Inter"; font.pixelSize: 14 * lay.k }
                Item { width: 1; height: (parent.height - 14 * lay.k * 1.2 * 2 - cardAction.height) / 2 }
                Text {
                    id: cardAction
                    text: lay.home.homeCard ? lay.home.homeCard.action : ""
                    color: lay.fgc
                    font.family: "Space Grotesk"
                    font.weight: Font.Bold
                    font.pixelSize: 44 * lay.k
                    font.letterSpacing: -0.02 * 44 * lay.k
                    lineHeight: 1.0
                }
                Item { width: 1; height: (parent.height - 14 * lay.k * 1.2 * 2 - cardAction.height) / 2 }
                Text { width: parent.width; elide: Text.ElideRight; text: lay.home.homeCard ? lay.home.homeCard.text : ""; color: lay.text3; font.family: "Inter"; font.pixelSize: 14 * lay.k }
            }
        }

        // The two app cards
        Row {
            visible: lay.home.homeCard === null
            width: parent.width
            height: 170 * lay.k
            spacing: 16 * lay.k
            Repeater {
                model: lay.home.cards
                delegate: HomeTile {
                    id: card
                    required property var modelData
                    home: lay.home
                    fid: modelData.id
                    k: lay.k
                    radius: 22 * lay.k
                    width: (parent.width - 16 * lay.k) / 2
                    height: 170 * lay.k
                    visible: modelData.present
                    dimmed: lay.home.sessionRunning && !modelData.running
                    glow: modelData.running
                    glowKind: "app"
                    moreSize: 38
                    moreRight: 141
                    moreTop: 14
                    moreBg: lay.veil(0.72)
                    moreFg: lay.fgc

                    Item {
                        anchors.fill: parent
                        layer.enabled: Theme.effects
                        layer.effect: MultiEffect { maskEnabled: true; maskSource: cardMask }
                        Rectangle { anchors.fill: parent; color: lay.surf }
                        Image {
                            id: cardArt
                            anchors.fill: parent
                            source: card.modelData.art
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            visible: !Theme.effects
                        }
                        MultiEffect {
                            visible: Theme.effects
                            anchors.fill: parent
                            source: cardArt
                            scale: 1.3
                            blurEnabled: true
                            blurMax: 64
                            blur: 1.0
                            saturation: 0.1
                        }
                        Rectangle {
                            anchors.fill: parent
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: lay.veil(0.92) }
                                GradientStop { position: 0.55; color: lay.veil(0.7) }
                                GradientStop { position: 1.0; color: lay.veil(0.35) }
                            }
                        }
                        Image {
                            anchors.right: parent.right
                            width: 127 * lay.k
                            height: parent.height
                            source: card.modelData.art
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }
                        Rectangle {
                            // glow overlay of the running card: linear-gradient(100deg, accent .42 -> .12 55% -> 0 80%)
                            visible: card.modelData.running
                            anchors.fill: parent
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: Qt.rgba(1, 0.416, 0.169, 0.42) }
                                GradientStop { position: 0.55; color: Qt.rgba(1, 0.416, 0.169, 0.12) }
                                GradientStop { position: 0.8; color: Qt.rgba(1, 0.416, 0.169, 0) }
                            }
                        }
                    }
                    Rectangle { id: cardMask; anchors.fill: parent; radius: 22 * lay.k; visible: false; layer.enabled: true }

                    Item {
                        x: 26 * lay.k
                        y: 20 * lay.k
                        width: parent.width - (26 + 150) * lay.k
                        height: parent.height - 40 * lay.k
                        Rectangle {
                            visible: card.modelData.running
                            height: 26 * lay.k
                            width: runRow.width + 24 * lay.k
                            radius: 13 * lay.k
                            color: "#FF6A2B"
                            Row {
                                id: runRow
                                anchors.centerIn: parent
                                spacing: 7 * lay.k
                                LiveDot { k: lay.k; color: "#0E0D0C"; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: qsTr("EN COURS · %1").arg(card.modelData.run); color: "#0E0D0C"; font.family: "Inter"; font.weight: Font.Bold; font.pixelSize: 12 * lay.k }
                            }
                        }
                        Text {
                            visible: !card.modelData.running
                            text: card.modelData.sub
                            color: lay.text3
                            font.family: "Inter"
                            font.pixelSize: 14 * lay.k
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            elide: Text.ElideRight
                            text: card.modelData.title
                            color: lay.fgc
                            font.family: "Space Grotesk"
                            font.weight: Font.Bold
                            font.pixelSize: 34 * lay.k
                            font.letterSpacing: -0.01 * 34 * lay.k
                        }
                        Row {
                            anchors.bottom: parent.bottom
                            spacing: 8 * lay.k
                            LavIcon { path: Theme.icons.play; filled: true; stroke: 0; size: 14 * lay.k; color: lay.accentText; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: card.modelData.action; color: lay.accentText; font.family: "Inter"; font.weight: Font.Bold; font.pixelSize: 15 * lay.k }
                        }
                    }
                }
            }
        }

        // « Bibliothèque Playnite · récents » + the focused game + « Tout voir → »
        Item {
            visible: lay.home.homeCard === null && !lay.home.noGames
            width: parent.width
            height: 10 * lay.k + caption.height
            Row {
                id: caption
                y: 10 * lay.k
                spacing: 14 * lay.k
                Text { text: qsTr("Bibliothèque Playnite · récents"); color: lay.text2; font.family: "Inter"; font.weight: Font.DemiBold; font.pixelSize: 14 * lay.k }
                Text { text: lay.home.heroTitle; color: Theme.muted; font.family: "Inter"; font.weight: Font.Medium; font.pixelSize: 14 * lay.k }
            }
            Text {
                anchors.right: parent.right
                y: 10 * lay.k
                text: qsTr("Tout voir →")
                color: Theme.muted
                font.family: "Inter"
                font.weight: Font.DemiBold
                font.pixelSize: 14 * lay.k
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: lay.home.act("lib") }
            }
        }
        Row {
            visible: lay.home.homeCard === null && !lay.home.noGames
            height: 180 * lay.k
            spacing: 14 * lay.k
            Repeater {
                model: lay.home.recents
                delegate: HomeTile {
                    id: cover
                    required property var modelData
                    required property int index
                    home: lay.home
                    fid: "r" + index
                    k: lay.k
                    radius: 14 * lay.k
                    width: 120 * lay.k
                    height: 180 * lay.k
                    dimmed: lay.home.sessionRunning && !lay.home.isLive(modelData)
                    glow: lay.home.isLive(modelData)
                    moreSize: 30
                    moreRight: 6
                    moreTop: 6
                    moreDots: 16
                    moreBg: lay.veil(0.78)
                    moreFg: lay.fgc
                    CoverFace { anchors.fill: parent; radius: 14 * lay.k; source: cover.modelData.cover; name: cover.modelData.name; bg: lay.surf; k: lay.k }
                }
            }
            HomeTile {
                home: lay.home
                fid: "lib"
                k: lay.k
                holdable: false
                radius: 14 * lay.k
                width: 120 * lay.k
                height: 180 * lay.k
                dimmed: lay.home.sessionRunning
                DashedRect {
                    anchors.fill: parent
                    radius: 14 * lay.k
                    fillColor: lay.dark ? Qt.rgba(26/255, 24/255, 22/255, 0.8) : Qt.rgba(1, 1, 1, 0.8)
                    borderWidth: 1.5 * lay.k
                    borderColor: lay.chipBorder
                }
                Text { x: 14 * lay.k; y: 14 * lay.k; text: lay.home.libTotal; color: Theme.muted; font.family: "Inter"; font.pixelSize: 13 * lay.k }
                Text {
                    x: 14 * lay.k
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 14 * lay.k
                    width: parent.width - 28 * lay.k
                    wrapMode: Text.WordWrap
                    text: qsTr("Tout voir →")
                    color: lay.fgc
                    font.family: "Space Grotesk"
                    font.weight: Font.Bold
                    font.pixelSize: 22 * lay.k
                    lineHeight: 1.05
                }
            }
        }
        NoGamesCard { visible: lay.home.homeCard === null && lay.home.noGames; k: lay.k; width: parent.width; height: 180 * lay.k; topMargin: 10 * lay.k }
    }
}
