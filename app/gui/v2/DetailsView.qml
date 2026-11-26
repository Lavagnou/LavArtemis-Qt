import QtQuick
import QtQuick.Controls
import QtQuick.Effects

import StreamingPreferences 1.0
import ProfileManager 1.0

// Details of a game or an app before launching it (prototype V8 « launch » screen, planches 67/68;
// design-port-spec §1.7): its picture, kind, title, hours or subtitle, the note that says what the
// launch will do, then « Réglages de ce lancement » (Profil, Écran, Codec, Débit) and the launch
// button. One layout per theme, as the prototype: Braise (picture behind a veil, floating panel),
// Affiche (picture band, black panel on the right), Ludique (one big card, « Comment tu veux
// jouer ? »). Prototype px x k.
//
// Rows change the real settings (the active profile, the virtual display, the codec, the bitrate),
// as the Settings page does: the prototype calls them "of this launch", the app has no per-launch
// settings yet (port-notes).
FocusScope {
    id: page
    objectName: "Détails"
    focus: true

    property var home: null
    // {appid, title, cover, bg, isGame, kindName, sub, installed, hours}
    property var target: ({})
    readonly property real k: 1.25

    readonly property bool joining: home ? (home.isRunningTarget(target) || home.inPlaynite(target)) : false
    readonly property bool warn: home ? home.closes(target) : false
    readonly property string hostName: home && home.host ? home.host.name : ""
    readonly property string note: {
        if (!home) return ""
        var n
        if (home.isRunningTarget(target)) n = qsTr("%1 tourne déjà sur %2 : %3").arg(target.title).arg(hostName)
                .arg(home.otherDevice ? qsTr("tu rejoins la session telle qu’elle est.") : qsTr("tu reprends la session telle qu’elle est."))
        else if (home.inPlaynite(target)) n = (home.playniteGame.length > 0 ? qsTr("%1 sera fermé, puis %2 se lance").arg(home.playniteGame).arg(target.title)
                                                                             : qsTr("%1 se lance").arg(target.title))
                                             + qsTr(" dans Playnite Fullscreen, déjà ouvert. Le stream continue.")
        else if (home.sessionRunning) n = home.closingText()
        else if (target.isGame) n = qsTr("Le jeu démarre directement sur %1.").arg(hostName)
        else n = qsTr("Rien ne tourne sur %1 pour l’instant.").arg(hostName)
        if (target.isGame && target.installed === false) n += " " + qsTr("Pas installé : Playnite proposera de l’installer.")
        return n
    }
    readonly property string sub: target.isGame ? (target.hours + " · " + (target.installed === false ? qsTr("pas installé") : qsTr("installé")))
                                                : (target.sub || "")
    readonly property string goLabel: !home ? "" : home.isRunningTarget(target) ? home.resumeLabel
                                    : home.inPlaynite(target) ? qsTr("Jouer dans Playnite") : home.playLabel(target.isGame)

    // ---- rows -----------------------------------------------------------------------------------
    readonly property var codecs: [StreamingPreferences.VCC_AUTO, StreamingPreferences.VCC_FORCE_AV1,
                                   StreamingPreferences.VCC_FORCE_HEVC, StreamingPreferences.VCC_FORCE_H264]
    readonly property var codecNames: [qsTr("Auto (HEVC)"), "AV1", "HEVC", "H.264"]
    readonly property var rates: [15000, 30000, 50000, 80000, 100000]
    property int rev: 0
    readonly property var profiles: { rev; var p = [{id: "", name: qsTr("Réglages globaux")}]; var l = ProfileManager.profiles; for (var i = 0; i < l.length; i++) p.push({id: l[i].id, name: l[i].name}); return p }
    function profileIndex() { for (var i = 0; i < profiles.length; i++) if (profiles[i].id === ProfileManager.activeProfileId) return i; return 0 }
    function codecIndex() { var i = codecs.indexOf(StreamingPreferences.videoCodecConfig); return i < 0 ? 0 : i }
    function rateIndex() {
        var best = 0
        for (var i = 0; i < rates.length; i++) if (Math.abs(rates[i] - StreamingPreferences.bitrateKbps) < Math.abs(rates[best] - StreamingPreferences.bitrateKbps)) best = i
        return best
    }
    readonly property var rows: {
        rev
        return [{label: qsTr("Profil"), value: profiles[profileIndex()].name},
                {label: qsTr("Écran"), value: joining || !StreamingPreferences.useVirtualDisplay ? qsTr("Écran du PC") : qsTr("Écran de cet appareil")},
                {label: qsTr("Codec"), value: StreamingPreferences.videoCodecConfig === StreamingPreferences.VCC_FORCE_PYROWAVE ? "PyroWave" : codecNames[codecIndex()]},
                {label: qsTr("Débit"), value: qsTr("%1 Mbit/s").arg(Math.round(StreamingPreferences.bitrateKbps / 1000))}]
    }
    function change(i, d) {
        if (i === 0) {
            var p = (profileIndex() + d + profiles.length) % profiles.length
            ProfileManager.setActiveProfile(profiles[p].id)
        } else if (i === 1) {
            if (joining) { toast.show(qsTr("Session en cours : le format suit l’écran du PC")); return }
            StreamingPreferences.useVirtualDisplay = !StreamingPreferences.useVirtualDisplay
        } else if (i === 2) {
            StreamingPreferences.videoCodecConfig = codecs[(codecIndex() + d + codecs.length) % codecs.length]
        } else if (i === 3) {
            StreamingPreferences.bitrateKbps = rates[Math.max(0, Math.min(rates.length - 1, rateIndex() + d))]
            StreamingPreferences.autoAdjustBitrate = false
        }
        StreamingPreferences.save()
        rev++
    }

    // ---- focus: o0..o3 then go ----------------------------------------------------------------------
    property string focusId: "go"
    function isFocused(id) { return focusId === id && Theme.input !== "touch" }
    MouseMoves { id: mouseMoves }
    function hover(id) { if (!mouseMoves.recent()) return; Theme.input = "mouse"; focusId = id }
    function act(id) {
        if (id === "go") { home.launchTarget(target); return }
        if (id === "back") { stackView.pop(); return }
        if (id[0] === "o") change(parseInt(id.substring(1)), 1)
    }
    function options() {}
    Keys.onPressed: (event) => {
        Theme.input = "keys"
        var ids = ["o0", "o1", "o2", "o3", "go"]
        var i = ids.indexOf(focusId)
        if (event.key === Qt.Key_Up) { focusId = ids[Math.max(0, i - 1)]; event.accepted = true }
        else if (event.key === Qt.Key_Down) { focusId = ids[Math.min(ids.length - 1, i + 1)]; event.accepted = true }
        else if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right) && focusId[0] === "o") {
            change(parseInt(focusId.substring(1)), event.key === Qt.Key_Left ? -1 : 1); event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            if (!event.isAutoRepeat) act(focusId)
            event.accepted = true
        } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) { stackView.pop(); event.accepted = true }
    }

    // ---- view ---------------------------------------------------------------------------------------
    readonly property bool dark: Theme.dark
    readonly property color text2: Theme.braise ? Theme.text2 : Theme.muted
    function veil(a) { return Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, a) }
    function lit(light, darkColor) { return page.dark ? darkColor : light }

    // The game's background, else its cover blurred (prototype BLUR: blur 28 px, saturate, x1.25)
    component TargetPicture: Item {
        clip: true
        Image {
            id: pic
            anchors.fill: parent
            source: page.target.bg || page.target.cover || ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: !Theme.effects
        }
        MultiEffect {
            visible: Theme.effects
            anchors.fill: parent
            source: pic
            blurEnabled: !page.target.bg
            blurMax: 64
            blur: page.target.bg ? 0 : 1.0
            scale: page.target.bg ? 1.0 : 1.25
        }
    }
    // ‹ › of a row of « Réglages de ce lancement »
    component RowArrow: Rectangle {
        id: arrow
        property int dir: 1
        property int row: 0
        property color fg: Theme.fg
        width: 36 * page.k; height: width
        opacity: row === 1 && page.joining ? 0.4 : 1
        Text { anchors.centerIn: parent; text: arrow.dir < 0 ? "‹" : "›"; color: arrow.fg; font.pixelSize: 18 * page.k; font.weight: Font.Black }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.change(arrow.row, arrow.dir) }
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }
    Stage {
        anchors.fill: parent

        // ===== Braise: picture behind a veil, cover + text left, panel right ============================
        Item {
            anchors.fill: parent
            visible: Theme.braise
            TargetPicture { anchors.fill: parent }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: page.veil(0.92) }
                    GradientStop { position: 0.5; color: page.veil(0.7) }
                    GradientStop { position: 1.0; color: page.veil(0.3) }
                }
            }
            HomeTile {
                home: page
                fid: "back"
                k: page.k
                holdable: false
                x: 40 * page.k; y: 24 * page.k
                width: 44 * page.k; height: 44 * page.k
                radius: 14 * page.k
                Rectangle { anchors.fill: parent; radius: parent.radius; color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.85) }
                LavIcon { anchors.centerIn: parent; name: "chevron-left"; size: 20 * page.k; color: Theme.fg }
            }
            Row {
                x: 40 * page.k
                y: 120 * page.k
                width: 600 * page.k
                spacing: 24 * page.k
                Item {
                    width: 150 * page.k; height: 225 * page.k
                    RectangularShadow { anchors.fill: parent; visible: Theme.effects; offset.y: 12 * page.k; blur: 30 * page.k; radius: 14 * page.k; color: Qt.rgba(0, 0, 0, 0.5) }
                    CoverFace { anchors.fill: parent; radius: 14 * page.k; source: page.target.cover || ""; name: page.target.title || ""; k: page.k }
                }
                Column {
                    width: parent.width - 174 * page.k
                    spacing: 10 * page.k
                    Text {
                        text: page.target.kindName + " · " + page.hostName
                        color: Theme.accentText
                        font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 14 * page.k
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: page.target.title || ""
                        color: Theme.fg
                        font.family: Theme.displayFont; font.weight: Theme.displayWeight; font.pixelSize: 48 * page.k
                        font.letterSpacing: -0.02 * 48 * page.k
                        lineHeight: 1.02
                    }
                    Text { text: page.sub; color: page.text2; font.family: Theme.textFont; font.pixelSize: 16 * page.k }
                    Item { width: 1; height: 0 }
                    Rectangle {
                        width: parent.width
                        height: noteText.implicitHeight + 28 * page.k
                        radius: 14 * page.k
                        color: page.warn ? Theme.warnBg : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.85)
                        border.width: page.warn ? page.k : 0
                        border.color: Theme.warnLine
                        Text {
                            id: noteText
                            x: 16 * page.k
                            width: parent.width - 32 * page.k
                            anchors.verticalCenter: parent.verticalCenter
                            wrapMode: Text.WordWrap
                            text: page.note
                            color: page.warn ? page.lit("#5A2A12", "#F3D9C9") : page.text2
                            lineHeight: 1.5
                            font.family: Theme.textFont; font.pixelSize: 14 * page.k
                        }
                    }
                }
            }
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 40 * page.k
                y: 96 * page.k
                width: 470 * page.k
                height: panel.height + 44 * page.k
                radius: 26 * page.k
                color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.94)
                border.width: Theme.border * page.k
                border.color: Theme.borderColor
                Column {
                    id: panel
                    x: 22 * page.k; y: 22 * page.k
                    width: parent.width - 44 * page.k
                    spacing: 10 * page.k
                    Text {
                        leftPadding: 6 * page.k; bottomPadding: 6 * page.k
                        text: qsTr("Réglages de ce lancement")
                        color: Theme.fg
                        font.family: Theme.displayFont; font.weight: Theme.displayWeight; font.pixelSize: 18 * page.k
                    }
                    Repeater {
                        model: Theme.braise ? page.rows : []
                        delegate: HomeTile {
                            id: row
                            required property var modelData
                            required property int index
                            home: page
                            fid: "o" + index
                            k: page.k
                            holdable: false
                            width: panel.width
                            height: 66 * page.k
                            radius: 16 * page.k
                            Rectangle { anchors.fill: parent; radius: parent.radius; color: Theme.surface2 }
                            Text {
                                x: 16 * page.k; width: 64 * page.k
                                anchors.verticalCenter: parent.verticalCenter
                                text: row.modelData.label
                                color: Theme.muted
                                font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 13 * page.k
                            }
                            RowArrow { dir: -1; row: row.index; x: 88 * page.k; anchors.verticalCenter: parent.verticalCenter; radius: 12 * page.k; color: Theme.line }
                            RowArrow { dir: 1; row: row.index; x: row.width - width - 8 * page.k; anchors.verticalCenter: parent.verticalCenter; radius: 12 * page.k; color: Theme.line }
                            Text {
                                x: 132 * page.k
                                width: row.width - 184 * page.k
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: row.modelData.value
                                color: Theme.fg
                                font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 14 * page.k
                            }
                        }
                    }
                    Item { width: 1; height: 0 }
                    HomeTile {
                        home: page
                        fid: "go"
                        k: page.k
                        holdable: false
                        width: panel.width
                        height: 64 * page.k
                        radius: 18 * page.k
                        Rectangle { anchors.fill: parent; radius: parent.radius; color: "#FF6A2B" }
                        Row {
                            anchors.centerIn: parent
                            spacing: 10 * page.k
                            LavIcon { path: Theme.icons.play; filled: true; stroke: 0; size: 18 * page.k; color: "#0E0D0C"; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: page.goLabel
                                color: "#0E0D0C"
                                font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 19 * page.k
                            }
                        }
                    }
                }
            }
        }

        // ===== Affiche: picture band + text in a 760 px column, black settings panel on the right ========
        Item {
            id: affiche
            anchors.fill: parent
            visible: Theme.affiche
            readonly property color ink: page.lit("#111111", "#EDEDE8")
            readonly property color panelBg: page.lit("#111111", "#EDEDE8")
            readonly property color panelFg: page.lit("#ECECE8", "#141414")
            Item {
                id: afficheLeft
                width: 760 * page.k
                height: parent.height
                Rectangle {
                    width: parent.width
                    height: 250 * page.k
                    color: affiche.ink
                    clip: true
                    TargetPicture { anchors.fill: parent; anchors.bottomMargin: 3 * page.k }
                    HomeTile {
                        home: page
                        fid: "back"
                        k: page.k
                        holdable: false
                        x: 24 * page.k; y: 20 * page.k
                        width: backLabel.implicitWidth + 24 * page.k
                        height: backLabel.implicitHeight + 16 * page.k
                        Rectangle { anchors.fill: parent; color: Theme.bg }
                        Text {
                            id: backLabel
                            anchors.centerIn: parent
                            text: qsTr("← Retour")
                            color: Theme.fg
                            font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 14 * page.k
                        }
                            }
                }
                Column {
                    x: 32 * page.k
                    y: (250 + 24) * page.k
                    width: parent.width - 64 * page.k
                    spacing: 12 * page.k
                    Text {
                        text: String(page.target.kindName || "").toUpperCase() + " — " + page.hostName
                        color: Theme.fg
                        font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 14 * page.k
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: page.target.title || ""
                        color: Theme.fg
                        font.family: Theme.displayFont; font.weight: Font.Black; font.pixelSize: 64 * page.k
                        font.letterSpacing: -0.05 * 64 * page.k
                        lineHeight: 0.92
                    }
                    Text { text: page.sub; color: Theme.muted; font.family: Theme.textFont; font.pixelSize: 16 * page.k }
                    Rectangle {
                        width: parent.width
                        height: afficheNote.implicitHeight + (page.warn ? 28 : 8) * page.k
                        color: page.warn ? "#FFD60A" : "transparent"
                        Rectangle { visible: !page.warn; width: 4 * page.k; height: parent.height; color: affiche.ink }
                        Text {
                            id: afficheNote
                            x: (page.warn ? 16 : 18) * page.k
                            width: parent.width - x - (page.warn ? 16 : 0) * page.k
                            anchors.verticalCenter: parent.verticalCenter
                            wrapMode: Text.WordWrap
                            text: page.note
                            color: page.warn ? "#111111" : Theme.fg
                            lineHeight: 1.45
                            font.family: Theme.textFont; font.weight: page.warn ? Font.DemiBold : Font.Normal; font.pixelSize: 15 * page.k
                        }
                    }
                }
            }
            Rectangle {
                anchors.right: parent.right
                width: 520 * page.k
                height: parent.height
                color: affiche.panelBg
                Text {
                    x: 32 * page.k; y: 28 * page.k
                    text: qsTr("RÉGLAGES DE CE LANCEMENT")
                    color: page.lit("#FFD60A", "#7A6000")
                    font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 14 * page.k
                }
                Column {
                    y: (28 + 14 + 17) * page.k
                    width: parent.width
                    Repeater {
                        model: Theme.affiche ? page.rows : []
                        delegate: HomeTile {
                            id: arow
                            required property var modelData
                            required property int index
                            home: page
                            fid: "o" + index
                            k: page.k
                            holdable: false
                            width: parent.width
                            height: 70 * page.k
                            readonly property color fg: arow.ringFill ? "#111111" : affiche.panelFg
                            Rectangle { anchors.fill: parent; color: arow.ringFill ? "#FFD60A" : "transparent" }
                            Rectangle { width: parent.width; height: page.k; color: page.lit("#333330", "#CFCFC9") }
                            Column {
                                x: 32 * page.k
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 4 * page.k
                                Text {
                                    text: arow.modelData.label.toUpperCase()
                                    color: arow.fg
                                    font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 12 * page.k
                                    font.letterSpacing: 0.06 * 12 * page.k
                                }
                                Text {
                                    text: arow.modelData.value
                                    color: arow.fg
                                    font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 19 * page.k
                                    font.letterSpacing: -0.01 * 19 * page.k
                                }
                            }
                            RowArrow { dir: -1; row: arow.index; fg: arow.fg; x: arow.width - 32 * page.k - 2 * width - 12 * page.k; anchors.verticalCenter: parent.verticalCenter; color: "transparent"; border.width: 2 * page.k; border.color: arow.fg }
                            RowArrow { dir: 1; row: arow.index; fg: arow.fg; x: arow.width - 32 * page.k - width; anchors.verticalCenter: parent.verticalCenter; color: "transparent"; border.width: 2 * page.k; border.color: arow.fg }
                        }
                    }
                }
                HomeTile {
                    home: page
                    fid: "go"
                    k: page.k
                    holdable: false
                    outlineOnly: true
                    x: 32 * page.k
                    y: parent.height - (28 + 72) * page.k
                    width: parent.width - 64 * page.k
                    height: 72 * page.k
                    Rectangle { anchors.fill: parent; color: "#FFD60A" }
                    Text {
                        x: 22 * page.k
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.goLabel
                        color: "#111111"
                        font.family: Theme.textFont; font.weight: Font.Black; font.pixelSize: 26 * page.k
                    }
                    Text {
                        anchors.right: parent.right; anchors.rightMargin: 22 * page.k
                        anchors.verticalCenter: parent.verticalCenter
                        text: "→"
                        color: "#111111"
                        font.family: Theme.textFont; font.weight: Font.Black; font.pixelSize: 26 * page.k
                    }
                }
            }
        }

        // ===== Ludique: one big card, picture on the left, « Comment tu veux jouer ? » on the right =======
        Rectangle {
            id: ludique
            visible: Theme.ludique
            x: 48 * page.k; y: 40 * page.k
            width: 1184 * page.k; height: 590 * page.k
            radius: 44 * page.k
            color: page.lit("#FFFFFF", "#221C3D")
            Item {
                // the left pane, cut to the card's rounded corners on the left only
                id: ludiqueLeft
                width: 480 * page.k
                height: parent.height
                Item {
                    id: ludiquePane
                    anchors.fill: parent
                    visible: !Theme.effects
                    Rectangle { anchors.fill: parent; color: page.lit("#1B1530", "#F3EEFF") }
                    TargetPicture { anchors.fill: parent }
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.4; color: page.lit(Qt.rgba(27/255, 21/255, 48/255, 0), Qt.rgba(0, 0, 0, 0)) }
                            GradientStop { position: 1.0; color: page.lit(Qt.rgba(27/255, 21/255, 48/255, 0.88), Qt.rgba(0, 0, 0, 0.88)) }
                        }
                    }
                    layer.enabled: Theme.effects
                }
                MultiEffect {
                    visible: Theme.effects
                    anchors.fill: parent
                    source: ludiquePane
                    maskEnabled: true
                    maskSource: ludiqueMask
                }
                Item {
                    id: ludiqueMask
                    anchors.fill: parent
                    visible: false
                    layer.enabled: Theme.effects
                    Rectangle { width: parent.width + 60 * page.k; height: parent.height; radius: 44 * page.k }
                }
                HomeTile {
                    home: page
                    fid: "back"
                    k: page.k
                    holdable: false
                    x: 20 * page.k; y: 20 * page.k
                    width: 52 * page.k; height: 52 * page.k
                    radius: 26 * page.k
                    Rectangle { anchors.fill: parent; radius: parent.radius; color: page.lit("#FFFFFF", "#221C3D") }
                    LavIcon { anchors.centerIn: parent; name: "chevron-left"; size: 22 * page.k; color: page.lit("#1B1530", "#F3EEFF") }
                    }
                Column {
                    x: 28 * page.k
                    width: parent.width - 56 * page.k
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 28 * page.k
                    spacing: 6 * page.k
                    Rectangle {
                        width: kindChip.implicitWidth + 24 * page.k
                        height: 28 * page.k
                        radius: 14 * page.k
                        color: "#C6F432"
                        Text { id: kindChip; anchors.centerIn: parent; text: page.target.kindName || ""; color: "#1B1530"; font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 13 * page.k }
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: page.target.title || ""
                        color: page.lit("#FFFFFF", "#F3EEFF")
                        font.family: Theme.displayFont; font.weight: Font.ExtraBold; font.pixelSize: 40 * page.k
                        font.letterSpacing: -0.03 * 40 * page.k
                        lineHeight: 1.0
                    }
                    Text { text: page.sub; color: page.lit("#E4DEFF", "#D9D2F5"); font.family: Theme.textFont; font.pixelSize: 15 * page.k }
                }
            }
            Item {
                id: ludiqueRight
                x: ludiqueLeft.width + 34 * page.k
                y: 30 * page.k
                width: parent.width - ludiqueLeft.width - 68 * page.k
                height: parent.height - 60 * page.k
                Column {
                    width: parent.width
                    spacing: 12 * page.k
                    Text {
                        text: qsTr("Comment tu veux jouer ?")
                        color: Theme.fg
                        font.family: Theme.displayFont; font.weight: Font.ExtraBold; font.pixelSize: 22 * page.k
                        font.letterSpacing: -0.02 * 22 * page.k
                    }
                    Repeater {
                        model: Theme.ludique ? page.rows : []
                        delegate: HomeTile {
                            id: lrow
                            required property var modelData
                            required property int index
                            home: page
                            fid: "o" + index
                            k: page.k
                            holdable: false
                            width: ludiqueRight.width
                            height: 64 * page.k
                            radius: 32 * page.k
                            Rectangle { anchors.fill: parent; radius: parent.radius; color: page.lit("#F3EEFF", "#15112A") }
                            Text {
                                x: 24 * page.k; width: 70 * page.k
                                anchors.verticalCenter: parent.verticalCenter
                                text: lrow.modelData.label
                                color: page.lit("#5A527A", "#B3A9D6")
                                font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 14 * page.k
                            }
                            RowArrow { dir: -1; row: lrow.index; x: 104 * page.k; width: 48 * page.k; radius: width / 2; anchors.verticalCenter: parent.verticalCenter; color: page.lit("#FFFFFF", "#221C3D") }
                            RowArrow { dir: 1; row: lrow.index; x: lrow.width - width - 8 * page.k; width: 48 * page.k; radius: width / 2; anchors.verticalCenter: parent.verticalCenter; color: page.lit("#FFFFFF", "#221C3D") }
                            Text {
                                x: 162 * page.k
                                width: lrow.width - 228 * page.k
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: lrow.modelData.value
                                color: Theme.fg
                                font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 16 * page.k
                            }
                        }
                    }
                    Rectangle {
                        width: parent.width
                        height: ludiqueNote.implicitHeight + 28 * page.k
                        radius: 24 * page.k
                        color: page.warn ? "#FF7A59" : page.lit("#E4DEFF", "#2E2650")
                        Text {
                            id: ludiqueNote
                            x: 20 * page.k
                            width: parent.width - 40 * page.k
                            anchors.verticalCenter: parent.verticalCenter
                            wrapMode: Text.WordWrap
                            text: page.note
                            color: page.warn ? "#1B1530" : page.lit("#1B1530", "#F3EEFF")
                            lineHeight: 1.4
                            font.family: Theme.textFont; font.weight: page.warn ? Font.DemiBold : Font.Normal; font.pixelSize: 15 * page.k
                        }
                    }
                }
                HomeTile {
                    home: page
                    fid: "go"
                    k: page.k
                    holdable: false
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 72 * page.k
                    radius: 36 * page.k
                    Rectangle { anchors.fill: parent; radius: parent.radius; color: page.lit("#5B3DF5", "#8C77FF") }
                    Row {
                        anchors.centerIn: parent
                        spacing: 12 * page.k
                        Rectangle {
                            width: 34 * page.k; height: width; radius: width / 2
                            color: "#C6F432"
                            anchors.verticalCenter: parent.verticalCenter
                            Text { anchors.centerIn: parent; text: "A"; color: "#1B1530"; font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 15 * page.k }
                        }
                        Text {
                            text: page.goLabel
                            color: page.lit("#FFFFFF", "#221C3D")
                            anchors.verticalCenter: parent.verticalCenter
                            font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 22 * page.k
                        }
                    }
                }
            }
        }

        HintsBar {
            k: page.k
            hints: page.home && page.home.optionsOpen ? [{k: "A", l: qsTr("Valider"), act: function() { page.home.pickOptions() }},
                    {k: "B", l: qsTr("Annuler"), act: function() { page.home.closeOptions() }}]
                 : [{k: "A", l: page.focusId === "go" ? page.goLabel : qsTr("Changer"), act: function() { page.act(page.focusId) }},
                    {k: "B", l: qsTr("Retour"), act: function() { stackView.pop() }}]
        }
        LavToast { id: toast; k: page.k }
    }
    StackView.onActivated: { page.focus = false; page.focus = true; forceActiveFocus() }
}
