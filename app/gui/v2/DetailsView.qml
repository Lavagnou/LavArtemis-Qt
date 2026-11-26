import QtQuick
import QtQuick.Controls
import QtQuick.Effects

import StreamingPreferences 1.0
import ProfileManager 1.0

// Details of a game or an app before launching it (prototype V8 « launch » screen, planches 67/68;
// design-port-spec §1.7): its picture, kind, title, hours or subtitle, the note that says what the
// launch will do, then « Réglages de ce lancement » (Profil, Écran, Codec, Débit) and the launch
// button. Layout of the Braise board for every theme, in the theme's colours (the Affiche and
// Ludique layouts of this screen are still to do: port-notes). Prototype px x k.
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
    readonly property var codecNames: [qsTr("Automatique (HEVC)"), "AV1", "HEVC", "H.264"]
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

    Rectangle { anchors.fill: parent; color: Theme.bg }
    Stage {
        anchors.fill: parent
        Item {
            anchors.fill: parent
            clip: true
            Image {
                id: bgImage
                anchors.fill: parent
                source: page.target.bg || page.target.cover || ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: !Theme.effects
            }
            MultiEffect {
                visible: Theme.effects
                anchors.fill: parent
                source: bgImage
                blurEnabled: !page.target.bg
                blurMax: 64
                blur: page.target.bg ? 0 : 1.0
                scale: page.target.bg ? 1.0 : 1.25
            }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: page.veil(0.92) }
                    GradientStop { position: 0.5; color: page.veil(0.7) }
                    GradientStop { position: 1.0; color: page.veil(0.3) }
                }
            }
        }

        HomeTile {
            home: page
            fid: "back"
            k: page.k
            holdable: false
            x: 40 * page.k; y: 24 * page.k
            width: 44 * page.k; height: 44 * page.k
            radius: Theme.affiche ? 0 : (Theme.ludique ? 22 : 14) * page.k
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
                RectangularShadow { anchors.fill: parent; visible: Theme.effects; offset.y: 12 * page.k; blur: 30 * page.k; radius: Theme.rs * page.k; color: Qt.rgba(0, 0, 0, 0.5) }
                CoverFace { anchors.fill: parent; radius: (Theme.affiche ? 0 : 14) * page.k; source: page.target.cover || ""; name: page.target.title || ""; k: page.k }
            }
            Column {
                width: parent.width - 174 * page.k
                spacing: 10 * page.k
                Text {
                    text: Theme.affiche ? (page.target.kindName + " — " + page.hostName).toUpperCase() : page.target.kindName + " · " + page.hostName
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
                    radius: Theme.affiche ? 0 : 14 * page.k
                    color: page.warn ? Theme.warnBg : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.85)
                    border.width: page.warn && Theme.braise ? page.k : 0
                    border.color: Theme.warnLine
                    Text {
                        id: noteText
                        x: 16 * page.k
                        width: parent.width - 32 * page.k
                        anchors.verticalCenter: parent.verticalCenter
                        wrapMode: Text.WordWrap
                        text: page.note
                        color: page.warn && Theme.braise ? (page.dark ? "#F3D9C9" : "#5A2A12") : (page.warn ? Theme.fg : page.text2)
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
            radius: (Theme.affiche ? 0 : (Theme.ludique ? 40 : 26)) * page.k
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
                    text: Theme.displayText(qsTr("Réglages de ce lancement"))
                    color: Theme.fg
                    font.family: Theme.displayFont; font.weight: Theme.displayWeight; font.pixelSize: 18 * page.k
                }
                Repeater {
                    model: page.rows
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
                        radius: (Theme.affiche ? 0 : 16) * page.k
                        Rectangle { anchors.fill: parent; radius: parent.radius; color: row.ringFill ? Theme.ringFillColor : Theme.surface2 }
                        Text {
                            x: 16 * page.k; width: 64 * page.k
                            anchors.verticalCenter: parent.verticalCenter
                            text: Theme.affiche ? row.modelData.label.toUpperCase() : row.modelData.label
                            color: row.ringFill ? Theme.ringFillFg : Theme.muted
                            font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 13 * page.k
                        }
                        Repeater {
                            model: [-1, 1]
                            delegate: Rectangle {
                                required property int modelData
                                x: modelData < 0 ? 88 * page.k : row.width - width - 8 * page.k
                                anchors.verticalCenter: parent.verticalCenter
                                width: (Theme.ludique ? 48 : 36) * page.k; height: width
                                radius: Theme.affiche ? 0 : (Theme.ludique ? width / 2 : 12 * page.k)
                                // Affiche: a 2 px frame; Ludique: white discs; Braise: a darker square
                                color: Theme.affiche ? "transparent" : (Theme.ludique ? Theme.surface : Theme.line)
                                border.width: Theme.affiche ? 2 * page.k : 0
                                border.color: Theme.fg
                                opacity: row.index === 1 && page.joining ? 0.4 : 1
                                Text { anchors.centerIn: parent; text: parent.modelData < 0 ? "‹" : "›"; color: Theme.fg; font.pixelSize: 18 * page.k; font.weight: Font.Bold }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.change(row.index, parent.modelData) }
                            }
                        }
                        Text {
                            x: 132 * page.k
                            width: row.width - 184 * page.k
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: row.modelData.value
                            color: row.ringFill ? Theme.ringFillFg : Theme.fg
                            font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 14 * page.k
                        }
                    }
                }
                Item { width: 1; height: 0 }
                HomeTile {
                    id: go
                    home: page
                    fid: "go"
                    k: page.k
                    holdable: false
                    width: panel.width
                    height: 64 * page.k
                    radius: (Theme.affiche ? 0 : (Theme.ludique ? 32 : 18)) * page.k
                    Rectangle { anchors.fill: parent; radius: parent.radius; color: Theme.affiche ? "#FFD60A" : (Theme.ludique ? (page.dark ? "#8C77FF" : "#5B3DF5") : "#FF6A2B") }
                    Row {
                        anchors.centerIn: parent
                        spacing: 10 * page.k
                        LavIcon { path: Theme.icons.play; filled: true; stroke: 0; size: 18 * page.k; color: goText.color; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            id: goText
                            text: Theme.affiche ? page.goLabel.toUpperCase() + " →" : page.goLabel
                            color: Theme.ludique ? "#FFFFFF" : (Theme.affiche ? "#111111" : "#0E0D0C")
                            font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 19 * page.k
                        }
                    }
                }
            }
        }
        HintsBar {
            k: page.k
            hints: [{k: "A", l: page.focusId === "go" ? page.goLabel : qsTr("Changer"), act: function() { page.act(page.focusId) }},
                    {k: "B", l: qsTr("Retour"), act: function() { stackView.pop() }}]
        }
        LavToast { id: toast; k: page.k }
    }
    StackView.onActivated: { page.focus = false; page.focus = true; forceActiveFocus() }
}
