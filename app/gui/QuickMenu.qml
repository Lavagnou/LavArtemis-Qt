import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Window
import SystemProperties 1.0
import "v2"

// The in-game menu (prototype V8 « ingame », planches 26, 28, 30, 31, 35, 61, 63, 66): an overlay of
// the stream window (QuickMenuManager covers it with this transparent window). Its entries are the
// user's (menu workshop of 2026-11-26, `dispositions/pc` version 89; LavArtemis'
// docs/superpowers/verif-2026-11-25/decisions-design.md « Menu en jeu »), the common ones in Android's
// order and wording (StreamMenu.tiles() / more()):
//  - tiles: Reprendre · Déconnecter · Quitter {jeu} (a game inside Playnite only: for the Bureau or
//    Playnite Fullscreen it would repeat « Quitter la session ») · Quitter la session · Passer en
//    trackpad / multitouch (a touch screen only) · Micro · Libérer la souris et le clavier ·
//    Statistiques de performance · Plus;
//  - « Plus »: Plein écran / fenêtre · Presse-papiers · Commandes du PC hôte · Zoom · Réduire la fenêtre ·
//    Capturer le clavier · Comportement de la souris (Bureau / Jeu), each with its icon.
// « Envoyer des touches », « Masquer le curseur local », « Verrouiller le pointeur » and « Coller du
// texte » are not in the menu (their Ctrl+Alt+Shift shortcuts stay). In the theme's layout:
//  - Braise: a scrim, the title and « PC · via » above a grid of 3 columns of tiles (icon, label, value);
//  - Affiche: a black panel on the right, one row per entry, no icons;
//  - Ludique: a dock of round tiles at the bottom (chip, short label, value), the PC pill at the top.
// Clipboard, host commands, « Plus » and the quit confirmation open the prototype's dialog
// (LavOptions). Prototype px x k (the Stage composes on a 1280 x 720 prototype scene x 1.25).
FocusScope {
    id: menu
    focus: true
    // The whole stream window (QuickMenuManager: SizeRootObjectToView); said again here, the view may not have
    // resized its root yet when it is first shown
    width: Window.width
    height: Window.height

    readonly property real k: 1.25
    property int rev: 0   // bumped at each opening and after an action: the values are read again
    property string focusId: "m0"

    // ---- entries ------------------------------------------------------------------------------------
    readonly property bool quitsGame: quickMenuManager.playniteGameName !== ""
    readonly property var ids: {
        var out = ["resume", "disconnect"]
        if (quitsGame) out.push("quit")
        out.push("quitSession")
        if (SystemProperties.hasTouchScreen) out.push("touch")
        out.push("mic", "release", "stats", "more")
        return out
    }
    // The workshop's ICON (= Android's drawable/v2_ic_m_*)
    readonly property var icons: ({
        resume: "M8 5l11 7-11 7z", clip: "M9 3h6v4H9zM7 5H5v16h14V5h-2",
        cmds: "M4 5h16v14H4zM8 10l3 2-3 2M13 14h3", zoom: "M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14zM20 20l-4-4M11 8v6M8 11h6",
        mic: "M12 3a3 3 0 0 1 3 3v5a3 3 0 0 1-6 0V6a3 3 0 0 1 3-3zM5 11a7 7 0 0 0 14 0M12 18v3", stats: "M5 20v-9M12 20V5M19 20v-6",
        more: "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 8v8M8 12h8", disconnect: "M15 4h4v16h-4M10 8l-4 4 4 4M6 12h10",
        quit: "M12 3v8M6.3 7.3a8 8 0 1 0 11.4 0", release: "M5 3l5.5 15 2.2-6.3L19 9.5zM13 13l5 5",
        quitSession: "M6 6l12 12M18 6L6 18",
        touch: "M9 11V5a2 2 0 0 1 4 0v6M13 10a2 2 0 0 1 4 0v3a7 7 0 0 1-7 7H9a5 5 0 0 1-4.6-3L3 14a1.6 1.6 0 0 1 2.7-1.6L9 15",
        fullscreen: "M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5", minimize: "M5 19h14",
        capture: "M3 6h18v12H3zM7 10h.01M11 10h.01M15 10h.01M8 14h8",
        mouseMode: "M12 3a6 6 0 0 1 6 6v6a6 6 0 0 1-12 0V9a6 6 0 0 1 6-6zM12 7v4"
    })
    readonly property string playingTitle: quitsGame ? quickMenuManager.playniteGameName : quickMenuManager.appTitle
    readonly property string via: {
        var kind = quickMenuManager.appKind
        var v = kind === "game" ? qsTr("Jeu Playnite")
              : kind === "playnite" ? (quitsGame ? qsTr("dans Playnite Fullscreen") : "Playnite Fullscreen")
              : kind === "desktop" ? qsTr("Bureau Windows") : ""
        return Theme.affiche ? v.toUpperCase() : v
    }
    readonly property string sep: Theme.affiche ? " — " : " · "
    readonly property string headLine: quickMenuManager.hostName + (via.length ? sep + via : "")

    // [label, short (Ludique), value, sub]
    function entry(id) {
        var micSent = quickMenuManager.microphoneActive()
        switch (id) {
        case "resume": return [qsTr("Reprendre"), qsTr("Reprendre"), "", ""]
        case "disconnect": return [qsTr("Déconnecter"), qsTr("Déconnecter"), "", ""]
        // only for a game inside Playnite (ids)
        case "quit": return [qsTr("Quitter %1").arg(playingTitle), qsTr("Quitter"), qsTr("Retour à Playnite"), qsTr("Retour à Playnite, le stream continue")]
        case "quitSession": return [qsTr("Quitter la session"), qsTr("Quitter la session"), "", ""]
        case "touch": return quickMenuManager.trackpadTouchMode() ? [qsTr("Passer en multitouch"), qsTr("Multitouch"), "", ""]
                                                                  : [qsTr("Passer en trackpad"), qsTr("Trackpad"), "", ""]
        case "stats": return [qsTr("Statistiques de performance"), qsTr("Stats"), quickMenuManager.isStatsVisible ? qsTr("Affichées") : qsTr("Masquées"), ""]
        case "mic": return micSent ? [qsTr("Micro"), qsTr("Micro"), quickMenuManager.microphoneMuted() ? qsTr("Coupé") : qsTr("Activé"), ""]
                                   : [qsTr("Micro non envoyé"), qsTr("Micro"), qsTr("Non envoyé"), ""]
        case "release": return [qsTr("Libérer la souris et le clavier"), qsTr("Libérer clavier/souris"), "", ""]
        case "more": return [qsTr("Plus"), qsTr("Plus"), "›", ""]
        }
        return [id, id, "", ""]
    }
    readonly property var entries: {
        rev
        var micSent = quickMenuManager.microphoneActive()
        var lu = Theme.ludique
        var out = []
        for (var i = 0; i < ids.length; i++) {
            var e = entry(ids[i])
            // Full labels already say « Micro non envoyé » and carry the Playnite subtitle on their own line
            var plain = !lu && ((ids[i] === "mic" && !micSent) || e[3] !== "")
            out.push({id: ids[i], label: lu ? e[1] : e[0], value: plain ? "" : e[2], sub: lu ? "" : e[3],
                      icon: icons[ids[i]] || "", dim: ids[i] === "mic" && !micSent})
        }
        return out
    }
    readonly property int cols: Theme.affiche ? 1 : (Theme.ludique ? ids.length : 3)
    // Every tile has the same size (user's rule, 2026-11-26): Braise's rows and Affiche's lines are all as tall
    // as the tallest tile (« Quitter Red Dead Redemption 2 » and its subtitle), never one row taller than the
    // others; Ludique's dock tiles have a fixed size. `needs` holds the height each tile needs, by layout.
    property var needs: ({})
    property int needsRev: 0
    function setNeed(key, h) { if (needs[key] !== h) { needs[key] = h; needsRev++ } }
    function tileHeight(layout, minimum) {
        needsRev
        var h = minimum
        for (var i = 0; i < ids.length; i++) {
            var n = needs[layout + i]
            if (n !== undefined) h = Math.max(h, n)
        }
        return h
    }

    // ---- focus (the prototype's grid) ---------------------------------------------------------------
    function isFocused(id) { return focusId === id && Theme.input !== "touch" }
    MouseMoves { id: mouseMoves }
    function hover(id) { if (!mouseMoves.recent()) return; Theme.input = "mouse"; focusId = id }
    function move(dx, dy) {
        var i = parseInt(focusId.substring(1)), n = ids.length
        // Affiche's list: Left / Right do nothing (audit O2); Ludique's dock (one row): Up / Down do nothing (G1)
        if (cols === 1) { if (dy === 0) return; i += dy }
        else if (dy !== 0) { var j = i + dy * cols; if (j >= 0 && j < n) i = j; else if (dy > 0 && Math.floor(i / cols) < Math.floor((n - 1) / cols)) i = n - 1 }
        else { var c = i % cols + dx; if (c >= 0 && c < cols && i + dx < n) i += dx }
        focusId = "m" + Math.max(0, Math.min(n - 1, i))
    }
    function act(fid) { doEntry(ids[parseInt(fid.substring(1))]) }
    function options() { return false }

    Keys.onPressed: (event) => {
        Theme.input = "keys"
        if (event.key === Qt.Key_Left) move(-1, 0)
        else if (event.key === Qt.Key_Right) move(1, 0)
        else if (event.key === Qt.Key_Up) move(0, -1)
        else if (event.key === Qt.Key_Down) move(0, 1)
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) { if (!event.isAutoRepeat) act(focusId) }
        else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Backspace) closeMenu()
        else return
        event.accepted = true
    }

    // ---- actions ------------------------------------------------------------------------------------
    function closeMenu() { quickMenuManager.hide() }
    // Said over the stream once the menu is closed (Session::showNotification)
    function closeAndSay(text) { closeMenu(); if (text) quickMenuManager.showToast(text) }

    function doEntry(id) {
        switch (id) {
        case "resume": closeMenu(); return
        case "clip":
            openDialog({title: qsTr("Presse-papiers"), note: qsTr("Copier-coller entre ce PC et %1.").arg(quickMenuManager.hostName),
                        entries: [{l: qsTr("Envoyer au PC ce que j’ai copié"), a: "clipboard_upload"},
                                  {l: qsTr("Récupérer ce qui est copié sur le PC"), a: "clipboard_fetch"}, cancelEntry]})
            return
        case "cmds": openCommands(); return
        case "stats": quickMenuManager.executeAction("toggle_stats"); closeMenu(); return
        case "mic":
            if (!quickMenuManager.microphoneActive()) {
                closeAndSay(qsTr("Le micro n’est pas envoyé : active « Envoyer mon micro au PC hôte » dans Réglages › Audio"))
                return
            }
            quickMenuManager.executeAction("toggle_microphone"); closeMenu(); return
        case "zoom":
            quickMenuManager.toggleZoom()
            closeAndSay(qsTr("Zoom : %1 %").arg(quickMenuManager.zoomPercent()))
            return
        case "release":
            quickMenuManager.executeAction("toggle_mouse")
            closeAndSay(qsTr("Souris et clavier libérés : Ctrl+Alt+Maj+Z pour les reprendre"))
            return
        case "more": openMore(); return
        case "disconnect": closeMenu(); quickMenuManager.executeAction("disconnect"); return
        case "quit": closeMenu(); quickMenuManager.quitPlayniteGame(); return
        case "quitSession": openQuitSession(); return
        case "minimize": closeMenu(); quickMenuManager.executeAction("minimize"); return
        case "mouseMode":
            quickMenuManager.executeAction("toggle_mouse_mode")
            closeAndSay(qsTr("Comportement de la souris : %1").arg(quickMenuManager.desktopMouseMode() ? qsTr("Bureau") : qsTr("Jeu")))
            return
        case "capture":
            quickMenuManager.executeAction("toggle_keyboard")
            closeAndSay(qsTr("Clavier capturé : les raccourcis système partent au PC distant"))
            return
        case "fullscreen":
            var wasFull = quickMenuManager.streamFullScreen()
            closeMenu()
            quickMenuManager.executeAction("toggle_fullscreen")
            quickMenuManager.showToast(wasFull ? qsTr("Stream en fenêtre") : qsTr("Stream en plein écran"))
            return
        case "touch":
            var trackpad = quickMenuManager.trackpadTouchMode()
            quickMenuManager.executeAction("toggle_touch_mode")
            closeAndSay(trackpad ? qsTr("Mode multitouch") : qsTr("Mode trackpad"))
            return
        }
    }

    // ---- dialogs (LavOptions) ---------------------------------------------------------------------------
    // As the prototype's isMain(): lists (keys, clipboard, commands, « Plus ») have no main entry, a
    // confirmation or a message has its first one
    readonly property var cancelEntry: ({l: qsTr("Annuler"), a: "cancel"})
    function dialogHead() {
        var kind = qsTr("Menu du stream")
        return Theme.affiche ? (kind + sep + quickMenuManager.hostName).toUpperCase() : kind + sep + quickMenuManager.hostName
    }
    function openDialog(cfg) {
        cfg.head = dialogHead()
        if (!cfg.warn) cfg.info = true
        dialog.showDialog(cfg)
    }
    function openCommands() {
        var names = quickMenuManager.serverCommandManager ? quickMenuManager.serverCommandManager.getAvailableCommands() : []
        if (names.length === 0) {
            openDialog({title: qsTr("Aucune commande de serveur"),
                        note: qsTr("Vérifie que le PC hôte lance Apollo ou LavApollo, et que cet appareil a la permission « Commande de serveur »."),
                        entries: [{l: qsTr("OK"), a: "cancel", main: true}]})
            return
        }
        var e = []
        for (var i = 0; i < names.length; i++) e.push({l: names[i], a: "server_cmd:" + names[i]})
        e.push(cancelEntry)
        openDialog({title: qsTr("Commandes du PC hôte"), note: qsTr("Accéder aux commandes de contrôle du PC hôte"), entries: e})
    }
    // « Plus »: the workshop's order, an icon before each entry (LavOptions: Braise accent, Ludique chip,
    // none in Affiche), a value after « · » as Android's StreamMenu.withValue(); two columns (kind "more": past 6
    // entries, as the prototype's ctxCols)
    function openMore() {
        var e = [{l: qsTr("Plein écran / fenêtre") + " · " + (quickMenuManager.streamFullScreen() ? qsTr("Plein écran") : qsTr("Fenêtre")), a: "do:fullscreen", icon: icons.fullscreen},
                 {l: qsTr("Presse-papiers"), a: "do:clip", icon: icons.clip},
                 {l: qsTr("Commandes du PC hôte"), a: "do:cmds", icon: icons.cmds},
                 {l: qsTr("Zoom") + " · " + qsTr("%1 %").arg(quickMenuManager.zoomPercent()), a: "do:zoom", icon: icons.zoom},
                 {l: qsTr("Réduire la fenêtre"), a: "do:minimize", icon: icons.minimize},
                 {l: qsTr("Capturer le clavier"), a: "do:capture", icon: icons.capture},
                 {l: qsTr("Comportement de la souris") + " · " + (quickMenuManager.desktopMouseMode() ? qsTr("Bureau") : qsTr("Jeu")), a: "do:mouseMode", icon: icons.mouseMode},
                 cancelEntry]
        openDialog({title: qsTr("Plus"), kind: "more", note: playingTitle + (via.length ? sep + via : ""), entries: e})
    }
    function openQuitSession() {
        openDialog({title: qsTr("Quitter %1 ?").arg(quitsGame ? quickMenuManager.appTitle : playingTitle), warn: true,
                    note: qsTr("Assure-toi d’avoir sauvegardé : quitter la session ferme l’appli qui tourne sur le PC."),
                    entries: [{l: qsTr("Quitter la session"), a: "quit", main: true}, cancelEntry]})
    }
    function onPicked(a) {
        if (a === "cancel") return
        if (a.indexOf("do:") === 0) { doEntry(a.substring(3)); return }
        if (a.indexOf("server_cmd:") === 0) { closeMenu(); quickMenuManager.serverCommandManager.executeCommand(a.substring(11)); return }
        if (a === "clipboard_upload" || a === "clipboard_fetch") { closeMenu(); quickMenuManager.executeAction(a); return }
        if (a === "quit") { closeMenu(); quickMenuManager.executeAction("quit"); return }
    }

    Connections {
        target: quickMenuManager
        function onPlayniteGameChanged() { menu.rev++ }
        function onStatsVisibilityChanged() { menu.rev++ }
        function onVisibilityChanged() {
            if (quickMenuManager.isVisible) {
                menu.focusId = "m0"
                menu.rev++
                if (dialog.opened) dialog.close()
                menu.forceActiveFocus()
            }
        }
    }
    Connections {
        target: quickMenuManager.serverCommandManager
        ignoreUnknownSignals: true
        // Said over the stream: the menu is closed by then
        function onCommandExecuted(commandName) { quickMenuManager.showToast(qsTr("Commande « %1 » envoyée").arg(commandName)) }
        function onCommandFailed(commandName, error) { quickMenuManager.showToast(qsTr("La commande « %1 » a échoué : %2").arg(commandName).arg(error)) }
    }
    Component.onCompleted: {
        forceActiveFocus()
        if (quickMenuManager.serverCommandManager) quickMenuManager.serverCommandManager.refreshCommands()
    }

    // ---- view ---------------------------------------------------------------------------------------------
    readonly property bool dark: Theme.dark
    function lit(light, darkColor) { return menu.dark ? darkColor : light }

    Stage {
        anchors.fill: parent

        // A click outside the menu closes it, as « Reprendre »
        MouseArea { anchors.fill: parent; onClicked: menu.closeMenu() }

        // ===== Braise: scrim, title, a grid of 3 columns ==============================================
        Rectangle {
            anchors.fill: parent
            visible: Theme.braise
            color: Qt.rgba(8 / 255, 7 / 255, 6 / 255, 0.62)
        }
        Column {
            id: braiseMenu
            visible: Theme.braise
            width: 740 * menu.k
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2 - 0.04 * height
            spacing: 16 * menu.k
            Item {
                width: parent.width
                height: braiseTitle.implicitHeight
                Text {
                    id: braiseTitle
                    text: menu.playingTitle
                    color: "#F3EEE8"
                    font.family: "Space Grotesk"; font.weight: Font.Bold; font.pixelSize: 26 * menu.k
                }
                Text {
                    anchors.right: parent.right
                    anchors.baseline: braiseTitle.baseline
                    text: menu.headLine
                    color: "#CFC6BC"
                    font.family: Theme.textFont; font.pixelSize: 14 * menu.k
                }
            }
            Grid {
                columns: 3
                columnSpacing: 12 * menu.k
                rowSpacing: 12 * menu.k
                Repeater {
                    model: Theme.braise ? menu.entries : []
                    delegate: HomeTile {
                        id: bt
                        required property var modelData
                        required property int index
                        home: menu
                        fid: "m" + index
                        k: menu.k
                        holdable: false
                        width: (740 - 24) / 3 * menu.k
                        // every row as tall as the tallest tile (min 96)
                        readonly property real need: Math.max(96 * menu.k, 24 * menu.k + 10 * menu.k + bLow.height + 32 * menu.k)
                        onNeedChanged: menu.setNeed("b" + index, need)
                        Component.onCompleted: menu.setNeed("b" + index, need)
                        height: menu.tileHeight("b", 96 * menu.k)
                        radius: 18 * menu.k
                        opacity: bt.modelData.dim ? 0.45 : 1
                        Rectangle { anchors.fill: parent; radius: parent.radius; color: menu.dark ? Qt.rgba(26 / 255, 24 / 255, 22 / 255, 0.95) : Qt.rgba(1, 1, 1, 0.95) }
                        // icon at the top, the label block at the bottom (justify-content: space-between)
                        LavIcon {
                            x: 18 * menu.k; y: 16 * menu.k
                            path: bt.modelData.icon; size: 24 * menu.k; stroke: 1.8
                            color: bt.modelData.dim ? menu.lit("#6B6158", "#CFC6BC") : menu.lit("#C9480F", "#FF8A57")
                        }
                        Item {
                            id: bLow
                            x: 18 * menu.k
                            width: parent.width - 36 * menu.k
                            height: Math.max(bLabels.implicitHeight, bValue.implicitHeight)
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 16 * menu.k
                            Column {
                                id: bLabels
                                width: parent.width - (bValue.implicitWidth > 0 ? bValue.implicitWidth + 8 * menu.k : 0)
                                spacing: 3 * menu.k
                                Text {
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    text: bt.modelData.label
                                    color: menu.lit("#1E1A16", "#F3EEE8")
                                    font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 16 * menu.k
                                }
                                Text {
                                    visible: bt.modelData.sub !== ""
                                    width: parent.width
                                    wrapMode: Text.WordWrap
                                    text: bt.modelData.sub
                                    color: menu.lit("#6B6158", "#CFC6BC")
                                    font.family: Theme.textFont; font.pixelSize: 12 * menu.k
                                }
                            }
                            Text {
                                id: bValue
                                anchors.right: parent.right
                                anchors.bottom: bLabels.verticalCenter
                                anchors.bottomMargin: -implicitHeight / 2
                                text: bt.modelData.value
                                color: menu.lit("#6B6158", "#CFC6BC")
                                font.family: Theme.textFont; font.pixelSize: 13 * menu.k
                            }
                        }
                    }
                }
            }
        }

        // ===== Affiche: a black panel on the right ========================================================
        Rectangle {
            id: affichePanel
            visible: Theme.affiche
            anchors.right: parent.right
            width: 420 * menu.k
            height: parent.height
            color: menu.lit("#111111", "#EDEDE8")
            readonly property color fg: menu.lit("#ECECE8", "#141414")
            MouseArea { anchors.fill: parent }  // a click on the panel does not close the menu
            Column {
                width: parent.width
                Item {
                    width: parent.width
                    height: afHead.implicitHeight + (24 + 16 + 3) * menu.k
                    Column {
                        id: afHead
                        x: 28 * menu.k; y: 24 * menu.k
                        width: parent.width - 56 * menu.k
                        spacing: 4 * menu.k
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: menu.headLine
                            color: menu.lit("#FFD60A", "#7A6000")
                            font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 13 * menu.k
                        }
                        Text {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: menu.playingTitle
                            color: affichePanel.fg
                            lineHeight: 1.0
                            font.family: Theme.displayFont; font.weight: Font.Black; font.pixelSize: 30 * menu.k
                            font.letterSpacing: -0.035 * 30 * menu.k
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 3 * menu.k; color: "#FFD60A" }
                }
                Repeater {
                    model: Theme.affiche ? menu.entries : []
                    delegate: HomeTile {
                        id: at
                        required property var modelData
                        required property int index
                        home: menu
                        fid: "m" + index
                        k: menu.k
                        holdable: false
                        width: affichePanel.width
                        // the prototype's row: the yellow fill only; its ring2 outline went past the window's edge
                        ringShown: false
                        // every line as tall as the tallest one (min 54)
                        readonly property real need: Math.max(54 * menu.k, aLabels.implicitHeight + 12 * menu.k)
                        onNeedChanged: menu.setNeed("a" + index, need)
                        Component.onCompleted: menu.setNeed("a" + index, need)
                        height: menu.tileHeight("a", 54 * menu.k)
                        opacity: at.modelData.dim ? 0.45 : 1
                        readonly property color fg: at.ringFill ? "#111111" : affichePanel.fg
                        Rectangle { anchors.fill: parent; color: at.ringFill ? "#FFD60A" : "transparent" }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: menu.k; color: menu.lit("#333330", "#CFCFC9") }
                        Column {
                            id: aLabels
                            x: 28 * menu.k
                            width: parent.width - 64 * menu.k - aValue.implicitWidth
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                text: at.modelData.label
                                color: at.fg
                                font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 20 * menu.k
                                font.letterSpacing: -0.02 * 20 * menu.k
                            }
                            Text {
                                visible: at.modelData.sub !== ""
                                text: at.modelData.sub
                                color: at.ringFill ? "#111111" : menu.lit("#A3A39C", "#555550")
                                font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 13 * menu.k
                            }
                        }
                        Text {
                            id: aValue
                            anchors.right: parent.right; anchors.rightMargin: 28 * menu.k
                            anchors.verticalCenter: parent.verticalCenter
                            text: at.modelData.value
                            color: at.fg
                            font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 14 * menu.k
                        }
                    }
                }
            }
        }

        // ===== Ludique: gradient, the PC pill at the top, a dock of round tiles ===========================
        Rectangle {
            anchors.fill: parent
            visible: Theme.ludique
            gradient: Gradient {
                GradientStop { position: 0.45; color: menu.dark ? Qt.rgba(0, 0, 0, 0) : Qt.rgba(27 / 255, 21 / 255, 48 / 255, 0) }
                GradientStop { position: 1.0; color: menu.dark ? Qt.rgba(0, 0, 0, 0.75) : Qt.rgba(27 / 255, 21 / 255, 48 / 255, 0.75) }
            }
        }
        Rectangle {
            visible: Theme.ludique
            x: 48 * menu.k; y: 36 * menu.k
            height: 52 * menu.k
            width: luHead.width + 30 * menu.k
            radius: height / 2
            color: menu.lit("#FFFFFF", "#221C3D")
            Row {
                id: luHead
                x: 8 * menu.k
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12 * menu.k
                Rectangle {
                    height: 36 * menu.k
                    width: luHost.implicitWidth + 28 * menu.k
                    radius: height / 2
                    color: menu.lit("#5B3DF5", "#8C77FF")
                    Text { id: luHost; anchors.centerIn: parent; text: quickMenuManager.hostName; color: menu.lit("#FFFFFF", "#221C3D"); font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 13 * menu.k }
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: menu.playingTitle; color: Theme.fg; font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 17 * menu.k }
                Text { anchors.verticalCenter: parent.verticalCenter; text: menu.via; color: menu.lit("#5A527A", "#B3A9D6"); font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 14 * menu.k }
            }
        }
        Rectangle {
            visible: Theme.ludique
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 76 * menu.k
            width: luDock.width + 32 * menu.k
            height: luDock.height + 28 * menu.k
            radius: 40 * menu.k
            color: menu.lit("#F3EEFF", "#15112A")
            MouseArea { anchors.fill: parent }
            Row {
                id: luDock
                anchors.centerIn: parent
                spacing: 8 * menu.k
                Repeater {
                    model: Theme.ludique ? menu.entries : []
                    delegate: HomeTile {
                        id: lt
                        required property var modelData
                        required property int index
                        home: menu
                        fid: "m" + index
                        k: menu.k
                        holdable: false
                        width: 96 * menu.k
                        height: 108 * menu.k
                        radius: 30 * menu.k
                        opacity: lt.modelData.dim ? 0.45 : 1
                        // CHIPS / CHIPS_DARK of the prototype: Reprendre lime, Déconnecter pink, Quitter coral
                        readonly property color chip: lt.modelData.id === "resume" ? "#C6F432"
                                                    : lt.modelData.id === "quit" ? "#FF7A59"
                                                    : lt.modelData.id === "disconnect" ? menu.lit("#FFE1D8", "#3A2430")
                                                    : menu.lit("#E4DEFF", "#2E2650")
                        readonly property color chipFg: lt.modelData.id === "resume" || lt.modelData.id === "quit" ? "#1B1530" : Theme.fg
                        Rectangle { anchors.fill: parent; radius: parent.radius; color: menu.lit("#FFFFFF", "#221C3D") }
                        Column {
                            anchors.centerIn: parent
                            spacing: 6 * menu.k
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 44 * menu.k; height: width; radius: width / 2
                                color: lt.chip
                                LavIcon { anchors.centerIn: parent; path: lt.modelData.icon; size: 22 * menu.k; stroke: 2; color: lt.chipFg }
                            }
                            // « Quitter la session », « Libérer clavier/souris » (workshop): two lines in a 96 px tile
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: lt.width - 12 * menu.k
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                lineHeight: 0.95
                                text: lt.modelData.label
                                color: Theme.fg
                                font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 13 * menu.k
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                height: 13 * menu.k
                                text: lt.modelData.value
                                color: menu.lit("#5A527A", "#B3A9D6")
                                font.family: Theme.textFont; font.weight: Font.DemiBold; font.pixelSize: 11 * menu.k
                            }
                        }
                    }
                }
            }
        }

        HintsBar {
            k: menu.k
            hints: [{k: "A", l: qsTr("Valider"), act: function() { menu.act(menu.focusId) }},
                    {k: "B", l: qsTr("Fermer le menu"), act: function() { menu.closeMenu() }}]
        }
    }

    LavOptions {
        id: dialog
        // the menu fills the stream window (the overlay may still be 0 x 0 offscreen)
        areaWidth: menu.width
        areaHeight: menu.height
        onPicked: (a) => menu.onPicked(a)
        returnFocusTo: menu
    }
}
