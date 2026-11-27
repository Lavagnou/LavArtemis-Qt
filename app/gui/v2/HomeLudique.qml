import QtQuick
import QtQuick.Effects

// Home, Ludique (prototype V8, Ludique light/dark home; design-port-spec §1.3): « Salut ! / On
// joue sur quoi ? », the host switcher as a dark capsule, the Scène as a capsule bar, the two app
// cards as coloured blocks with a tilted art, then the recent covers and « Tout voir ». Dark mode
// turns the navy capsules light. Prototype px x k (1.25).
Item {
    id: lay

    required property var home
    readonly property real k: home.k
    readonly property bool dark: Theme.dark
    readonly property color fgc: dark ? "#F3EEFF" : "#1B1530"
    readonly property color mutedc: dark ? "#B3A9D6" : "#5A527A"
    // the navy capsules (switcher, Scène, card pills) and their text
    readonly property color capsule: dark ? "#F3EEFF" : "#1B1530"
    readonly property color capsuleInk: dark ? "#221C3D" : "#FFFFFF"
    readonly property color capsuleSoft: dark ? "#5A527A" : "#D9D2F5"
    readonly property color lime: dark ? "#3F5A00" : "#C6F432"
    // A window lower than the prototype's scene (1280 x 720; 1000 x 600, or a maximised window on a 16:9 screen, is
    // 600 to 660 px of it): the recents keep 28 px above the hints bar (44 px, 20 from the bottom: what the prototype
    // leaves at 720), else the bar covered the library tile (scenario 21). The gaps close up (the one above the bar
    // down to 14: the recents' ring, 4 + 5 px x 1.04, still clears it), then the cards (up to 20 px: their « En
    // cours » pill reaches the title), then the covers (186 -> 120, at their 2:3), so that the recents and their
    // focus ring stay inside the Stage, which clips, and out from under the bar
    readonly property real deficit: Math.max(0, ((home.stageOn ? 442 : 404) + 186 + 28 + 64) * k - height) / k
    function take(before, slack) { return Math.min(Math.max(0, deficit - before), slack) }
    readonly property real upBar: home.stageOn ? take(0, 8) : 0
    readonly property real upApps: upBar + take(upBar, home.stageOn ? 6 : 30)
    readonly property real gapLab: take(upApps, 12)
    readonly property real gapRec: take(upApps + gapLab, 12)
    readonly property real gapBar: take(upApps + gapLab + gapRec, 14)
    readonly property real cardCut: take(upApps + gapLab + gapRec + gapBar, 20)
    readonly property real coverCut: take(upApps + gapLab + gapRec + gapBar + cardCut, 66)
    readonly property real coverH: (186 - coverCut) * k
    readonly property real coverW: coverH * 2 / 3
    readonly property real upLab: upApps + cardCut + gapLab
    readonly property real upRec: upLab + gapRec
    readonly property real cardH: (204 - cardCut) * k
    readonly property real appsTop: ((home.stageOn ? 180 : 128) - upApps) * k
    readonly property real labTop: ((home.stageOn ? 402 : 362) - upLab) * k
    readonly property real recTop: ((home.stageOn ? 442 : 404) - upRec) * k

    // ---- header ------------------------------------------------------------------------------------
    Item {
        id: header
        x: 48 * lay.k
        y: 26 * lay.k
        width: parent.width - 96 * lay.k
        height: 58 * lay.k

        Row {
            id: brand
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14 * lay.k
            ThemeLogo { size: 52 * lay.k; anchors.verticalCenter: parent.verticalCenter }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Row {
                    spacing: 8 * lay.k
                    Text { text: qsTr("Salut !"); color: lay.mutedc; font.family: "Bricolage Grotesque"; font.weight: Font.DemiBold; font.pixelSize: 16 * lay.k; anchors.verticalCenter: parent.verticalCenter }
                    UpdateButton { k: lay.k; anchors.verticalCenter: parent.verticalCenter }
                }
                Text {
                    text: qsTr("On joue sur quoi ?")
                    color: lay.fgc
                    font.family: "Bricolage Grotesque"
                    font.weight: Font.ExtraBold
                    font.pixelSize: 34 * lay.k
                    font.letterSpacing: -0.03 * 34 * lay.k
                    lineHeight: 1.05
                }
            }
        }
        readonly property real leftW: Math.max(420 * lay.k, brand.width)
        readonly property real rightW: Math.max(360 * lay.k, buttons.width)

        Rectangle {
            id: switcher
            visible: lay.home.host !== null
            height: 58 * lay.k
            width: swRow.width + 16 * lay.k
            radius: height / 2
            color: lay.capsule
            x: header.leftW + (parent.width - header.leftW - header.rightW - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            Row {
                id: swRow
                x: 8 * lay.k
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10 * lay.k
                SwitchButton { text: lay.home.lbLabel; onClicked: lay.home.shoulder(-1) }
                Column {
                    width: Math.max(170 * lay.k, implicitWidth)
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: lay.home.host ? lay.home.host.name : ""
                        color: lay.capsuleInk
                        font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 17 * lay.k
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: lay.home.hostStatus
                        color: lay.capsuleSoft
                        font.family: "Bricolage Grotesque"; font.pixelSize: 12 * lay.k
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
        opacity: lay.home.hosts.length >= 2 ? 1 : 0
        height: 42 * lay.k
        width: sbText.implicitWidth + 28 * lay.k
        radius: height / 2
        color: lay.dark ? "#DCD3FF" : "#2E2650"
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        Text { id: sbText; anchors.centerIn: parent; text: sb.text; color: lay.capsuleSoft; font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 13 * lay.k }
        MouseArea { anchors.fill: parent; enabled: sb.opacity > 0; cursorShape: Qt.PointingHandCursor; onClicked: sb.clicked() }
    }

    // ---- Scène: capsule bar ------------------------------------------------------------------------------
    Rectangle {
        visible: lay.home.stageOn
        x: 48 * lay.k
        y: (100 - lay.upBar) * lay.k
        width: parent.width - 96 * lay.k
        height: 64 * lay.k
        radius: height / 2
        color: lay.capsule
        Row {
            x: 24 * lay.k
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 24 * lay.k - stageButtons.width - 9 * lay.k - 16 * lay.k
            spacing: 16 * lay.k
            Row {
                id: liveRow
                spacing: 7 * lay.k
                anchors.verticalCenter: parent.verticalCenter
                LiveDot { k: lay.k; color: lay.lime; anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("En cours · %1").arg(lay.home.stageTime); color: lay.lime; font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 13 * lay.k }
            }
            // A long title gets smaller (down to 17 px) to stay on one line, then takes a second one inside the
            // capsule; elided only past two lines
            FitTitle {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(naturalWidth, parent.width - liveRow.width - viaText.width - 32 * lay.k)
                text: lay.home.stageTitle
                color: lay.capsuleInk
                maxSize: 24 * lay.k
                minSize: 17 * lay.k
                spacingEm: -0.02
                font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold
            }
            Text {
                id: viaText
                anchors.verticalCenter: parent.verticalCenter
                text: lay.home.stageVia
                color: lay.capsuleSoft
                font.family: "Bricolage Grotesque"; font.weight: Font.DemiBold; font.pixelSize: 14 * lay.k
            }
        }
        Row {
            id: stageButtons
            anchors.right: parent.right
            anchors.rightMargin: 9 * lay.k
            anchors.verticalCenter: parent.verticalCenter
            spacing: 16 * lay.k
            HomeTile {
                home: lay.home
                fid: "resume"
                k: lay.k
                holdable: false
                radius: 23 * lay.k
                width: resumeRow.width + 44 * lay.k
                height: 46 * lay.k
                Rectangle { anchors.fill: parent; radius: parent.height / 2; color: "#C6F432" }
                Row {
                    id: resumeRow
                    anchors.centerIn: parent
                    spacing: 8 * lay.k
                    LavIcon { path: Theme.icons.play; filled: true; stroke: 0; size: 14 * lay.k; color: "#1B1530"; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: lay.home.resumeLabel; color: "#1B1530"; font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 16 * lay.k }
                }
            }
            HomeTile {
                home: lay.home
                fid: "squit"
                k: lay.k
                holdable: false
                radius: 23 * lay.k
                width: quitText.implicitWidth + 40 * lay.k
                height: 46 * lay.k
                Rectangle { anchors.fill: parent; radius: parent.height / 2; color: "transparent"; border.width: 2 * lay.k; border.color: lay.capsuleInk }
                Text { id: quitText; anchors.centerIn: parent; text: qsTr("Quitter"); color: lay.capsuleInk; font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 16 * lay.k }
            }
        }
    }

    // ---- offline / first run / searching / loading ----------------------------------------------------------
    HomeTile {
        visible: lay.home.homeCard !== null
        home: lay.home
        fid: "wake"
        k: lay.k
        holdable: false
        radius: 40 * lay.k
        x: 48 * lay.k
        y: 140 * lay.k
        width: Math.min(680 * lay.k, Math.max(560 * lay.k, wakeAction.implicitWidth + 66 * lay.k))
        height: 220 * lay.k
        DashedRect {
            anchors.fill: parent
            radius: 40 * lay.k
            borderWidth: 3 * lay.k
            borderColor: lay.dark ? "#4A3F7A" : "#B9AEE6"
            fillColor: lay.dark ? "#221C3D" : "#FFFFFF"
        }
        Text { x: 30 * lay.k; y: 30 * lay.k; text: lay.home.homeCard ? lay.home.homeCard.title : ""; color: lay.mutedc; font.family: "Bricolage Grotesque"; font.weight: Font.Bold; font.pixelSize: 16 * lay.k }
        Text {
            id: wakeAction
            x: 30 * lay.k
            anchors.verticalCenter: parent.verticalCenter
            text: lay.home.homeCard ? lay.home.homeCard.action : ""
            color: lay.fgc
            font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 40 * lay.k
            font.letterSpacing: -0.03 * 40 * lay.k
        }
        Text {
            x: 30 * lay.k
            width: parent.width - 60 * lay.k
            elide: Text.ElideRight
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 30 * lay.k
            text: lay.home.homeCard ? lay.home.homeCard.text : ""
            color: Theme.muted
            font.family: "Bricolage Grotesque"; font.weight: Font.DemiBold; font.pixelSize: 15 * lay.k
        }
    }

    // ---- the two app cards: coloured blocks ------------------------------------------------------------------
    Row {
        visible: lay.home.homeCard === null
        x: 48 * lay.k
        y: lay.appsTop
        width: parent.width - 96 * lay.k
        height: lay.cardH
        spacing: 18 * lay.k
        Repeater {
            model: lay.home.cards
            delegate: HomeTile {
                id: card
                required property var modelData
                home: lay.home
                fid: modelData.id
                k: lay.k
                radius: 36 * lay.k
                width: (parent.width - 18 * lay.k) / 2
                height: lay.cardH
                visible: modelData.present
                dimmed: lay.home.sessionRunning && !modelData.running
                glow: modelData.running
                glowKind: "app"
                moreSize: 40; moreRight: 14; moreTop: 14; moreDots: 18
                moreBg: lay.dark ? "#221C3D" : "#FFFFFF"
                moreFg: lay.dark ? "#F3EEFF" : "#1B1530"
                moreShadow: true
                moreShadowColor: lay.dark ? Qt.rgba(0, 0, 0, 0.25) : Qt.rgba(27/255, 21/255, 48/255, 0.25)

                Rectangle { anchors.fill: parent; radius: 36 * lay.k; color: card.modelData.kind === "desktop" ? "#FF7A59" : "#C6F432" }
                Item {
                    // tilted art, 118 x 158, 70 from the right, 22 from the top
                    x: parent.width - 70 * lay.k - width
                    y: 22 * lay.k
                    width: 118 * lay.k
                    height: 158 * lay.k
                    rotation: 6
                    RectangularShadow {
                        anchors.fill: parent
                        visible: Theme.effects
                        offset.y: 10 * lay.k
                        blur: 24 * lay.k
                        radius: 22 * lay.k
                        color: lay.dark ? Qt.rgba(0, 0, 0, 0.3) : Qt.rgba(27/255, 21/255, 48/255, 0.3)
                    }
                    CoverFace { anchors.fill: parent; radius: 22 * lay.k; source: card.modelData.art; name: ""; bg: "transparent"; k: lay.k }
                }
                Rectangle {
                    visible: card.modelData.running
                    x: 24 * lay.k
                    y: 22 * lay.k
                    height: 30 * lay.k
                    width: runRow.width + 28 * lay.k
                    radius: height / 2
                    color: lay.capsule
                    Row {
                        id: runRow
                        anchors.centerIn: parent
                        spacing: 7 * lay.k
                        LiveDot { k: lay.k; color: lay.lime; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: qsTr("En cours · %1").arg(card.modelData.run); color: lay.lime; font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 13 * lay.k }
                    }
                }
                Column {
                    x: 24 * lay.k
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 24 * lay.k
                    spacing: 4 * lay.k
                    Text {
                        text: card.modelData.title
                        color: "#1B1530"
                        font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 34 * lay.k
                        font.letterSpacing: -0.03 * 34 * lay.k
                        lineHeight: 1.0
                    }
                    Text {
                        text: card.modelData.kind === "desktop" ? qsTr("Ton bureau Windows, tel quel") : qsTr("Plein écran, choisis ton jeu à la manette")
                        color: "#1B1530"
                        font.family: "Bricolage Grotesque"; font.weight: Font.DemiBold; font.pixelSize: 15 * lay.k
                    }
                    Item { width: 1; height: 4 * lay.k }
                    Rectangle {
                        height: 42 * lay.k
                        width: actRow.width + 40 * lay.k
                        radius: height / 2
                        color: lay.capsule
                        Row {
                            id: actRow
                            anchors.centerIn: parent
                            spacing: 8 * lay.k
                            LavIcon { path: Theme.icons.play; filled: true; stroke: 0; size: 14 * lay.k; color: lay.capsuleInk; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: card.modelData.action; color: lay.capsuleInk; font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 16 * lay.k }
                        }
                    }
                }
            }
        }
    }

    // ---- section label --------------------------------------------------------------------------------------
    Item {
        visible: lay.home.homeCard === null && !lay.home.noGames
        x: 48 * lay.k
        y: lay.labTop
        width: parent.width - 96 * lay.k
        height: sectionText.implicitHeight
        Row {
            spacing: 14 * lay.k
            Text {
                id: sectionText
                text: qsTr("Bibliothèque Playnite · récents")
                color: lay.fgc
                font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 20 * lay.k
                font.letterSpacing: -0.02 * 20 * lay.k
            }
            Text {
                y: sectionText.baselineOffset - baselineOffset
                text: lay.home.heroTitle
                color: lay.mutedc
                font.family: "Bricolage Grotesque"; font.weight: Font.Medium; font.pixelSize: 15 * lay.k
            }
        }
        Text {
            anchors.right: parent.right
            y: sectionText.baselineOffset - baselineOffset
            text: qsTr("Tout voir →")
            color: Theme.muted
            font.family: "Bricolage Grotesque"; font.weight: Font.Bold; font.pixelSize: 15 * lay.k
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: lay.home.act("lib") }
        }
    }

    // ---- recents + library tile -------------------------------------------------------------------------------
    Row {
        visible: lay.home.homeCard === null && !lay.home.noGames
        x: 48 * lay.k
        y: lay.recTop
        spacing: 16 * lay.k
        Repeater {
            model: lay.home.recents
            delegate: HomeTile {
                id: cover
                required property var modelData
                required property int index
                home: lay.home
                fid: "r" + index
                k: lay.k
                radius: 24 * lay.k
                width: lay.coverW
                height: lay.coverH
                dimmed: lay.home.sessionRunning && !lay.home.isLive(modelData)
                glow: lay.home.isLive(modelData)
                moreSize: 32; moreRight: 8; moreTop: 8; moreDots: 18
                moreBg: "#FFFFFF"
                moreFg: "#1B1530"
                moreShadow: true
                moreShadowColor: lay.dark ? Qt.rgba(0, 0, 0, 0.25) : Qt.rgba(27/255, 21/255, 48/255, 0.25)
                CoverFace { anchors.fill: parent; radius: 24 * lay.k; source: cover.modelData.cover; name: cover.modelData.name; bg: lay.dark ? "#2E2650" : "#E4DEFF"; k: lay.k }
            }
        }
        HomeTile {
            home: lay.home
            fid: "lib"
            k: lay.k
            holdable: false
            radius: 24 * lay.k
            width: lay.coverW
            height: lay.coverH
            dimmed: lay.home.sessionRunning
            Rectangle { anchors.fill: parent; radius: 24 * lay.k; color: "transparent"; border.width: 3 * lay.k; border.color: lay.fgc }
            Text { x: 16 * lay.k; y: 16 * lay.k; text: lay.home.libTotal; color: lay.fgc; opacity: 0.75; font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 13 * lay.k }
            Text {
                x: 16 * lay.k
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16 * lay.k
                width: parent.width - 32 * lay.k
                wrapMode: Text.WordWrap
                text: qsTr("Tout voir →")
                color: lay.fgc
                font.family: "Bricolage Grotesque"; font.weight: Font.ExtraBold; font.pixelSize: 22 * lay.coverH / 186
                lineHeight: 1.05
            }
        }
    }
    NoGamesCard {
        visible: lay.home.homeCard === null && lay.home.noGames
        x: 48 * lay.k
        y: lay.labTop
        k: lay.k
        width: parent.width - 96 * lay.k
        height: (226 - lay.coverCut) * lay.k
    }
}
