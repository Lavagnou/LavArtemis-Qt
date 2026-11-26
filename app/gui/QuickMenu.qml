import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Window
import KeyMacroManager 1.0
import SystemProperties 1.0
import "v2"

// The in-game menu (prototype V8 « ingame », planches 26, 28, 30, 31, 35, 61, 63, 66): an overlay of
// the stream window (QuickMenuManager covers it with this transparent window), 11 tiles = Reprendre,
// the 9 most useful entries and « Plus », in the theme's layout:
//  - Braise: a scrim, the title and « PC · via » above a grid of 3 columns of tiles (icon, label, value);
//  - Affiche: a black panel on the right, one row per entry, no icons;
//  - Ludique: a dock of 11 round tiles at the bottom (chip, short label, value), the PC pill at the top.
// Keys, clipboard, host commands, « Plus » and the quit confirmation open the prototype's dialog
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
    readonly property var ids: ["resume", "keys", "clip", "cmds", "stats", "mic", "zoom", "release", "more", "disconnect", "quit"]
    readonly property var icons: ({
        resume: "M8 5l11 7-11 7z", keys: "M3 6h18v12H3zM7 10h.01M11 10h.01M15 10h.01M8 14h8", clip: "M9 3h6v4H9zM7 5H5v16h14V5h-2",
        cmds: "M4 5h16v14H4zM8 10l3 2-3 2M13 14h3", zoom: "M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14zM20 20l-4-4M11 8v6M8 11h6",
        mic: "M12 3a3 3 0 0 1 3 3v5a3 3 0 0 1-6 0V6a3 3 0 0 1 3-3zM5 11a7 7 0 0 0 14 0M12 18v3", stats: "M5 20v-9M12 20V5M19 20v-6",
        more: "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 8v8M8 12h8", disconnect: "M15 4h4v16h-4M10 8l-4 4 4 4M6 12h10",
        quit: "M12 3v8M6.3 7.3a8 8 0 1 0 11.4 0", release: "M5 3l5.5 15 2.2-6.3L19 9.5zM13 13l5 5"
    })
    readonly property bool quitsGame: quickMenuManager.playniteGameName !== ""
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
        case "keys": return [qsTr("Envoyer des touches"), qsTr("Touches"), "›", ""]
        case "clip": return [qsTr("Presse-papiers"), qsTr("Copier"), "›", ""]
        case "cmds": return [qsTr("Commandes du PC hôte"), qsTr("Commandes"), "›", ""]
        case "stats": return [qsTr("Statistiques de performance"), qsTr("Stats"), quickMenuManager.isStatsVisible ? qsTr("Affichées") : qsTr("Masquées"), ""]
        case "mic": return micSent ? [qsTr("Micro"), qsTr("Micro"), quickMenuManager.microphoneMuted() ? qsTr("Coupé") : qsTr("Activé"), ""]
                                   : [qsTr("Micro non envoyé"), qsTr("Micro"), qsTr("Non envoyé"), ""]
        case "zoom": return [qsTr("Zoom"), qsTr("Zoom"), qsTr("%1 %").arg(quickMenuManager.zoomPercent()), ""]
        case "release": return [qsTr("Libérer la souris et le clavier"), qsTr("Libérer"), "", ""]
        case "more": return [qsTr("Plus"), qsTr("Plus"), "›", ""]
        case "disconnect": return [qsTr("Déconnecter"), qsTr("Déconnecter"), "", ""]
        case "quit": return quitsGame ? [qsTr("Quitter %1").arg(playingTitle), qsTr("Quitter"), qsTr("Retour à Playnite"), qsTr("Retour à Playnite, le stream continue")]
                                      : [qsTr("Quitter %1").arg(playingTitle), qsTr("Quitter"), "", ""]
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
    readonly property int cols: Theme.affiche ? 1 : (Theme.ludique ? 11 : 3)
    // Braise: the height each tile needs, so that a row of the grid is as tall as its tallest tile
    property var needs: ({})
    property int needsRev: 0
    function setNeed(i, h) { if (needs[i] !== h) { needs[i] = h; needsRev++ } }
    function rowHeight(r) {
        needsRev
        var h = 96 * k
        for (var i = r * 3; i < r * 3 + 3; i++) if (needs[i] !== undefined) h = Math.max(h, needs[i])
        return h
    }

    // ---- focus (the prototype's grid) ---------------------------------------------------------------
    function isFocused(id) { return focusId === id && Theme.input !== "touch" }
    MouseMoves { id: mouseMoves }
    function hover(id) { if (!mouseMoves.recent()) return; Theme.input = "mouse"; focusId = id }
    function move(dx, dy) {
        var i = parseInt(focusId.substring(1)), n = ids.length
        if (cols === 1) i += dy !== 0 ? dy : dx
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
        case "keys": openKeys(); return
        case "clip":
            openDialog({title: qsTr("Presse-papiers"), note: qsTr("Copier-coller entre ce PC et %1.").arg(quickMenuManager.hostName),
                        entries: [{l: qsTr("Envoyer au PC ce que j’ai copié"), a: "clipboard_upload"},
                                  {l: qsTr("Récupérer ce qui est copié sur le PC"), a: "clipboard_fetch"}, cancelEntry]})
            return
        case "cmds": openCommands(); return
        case "stats": quickMenuManager.executeAction("toggle_stats"); closeMenu(); return
        case "mic":
            if (!quickMenuManager.microphoneActive()) {
                closeAndSay(qsTr("Le micro n’est pas envoyé : active « Envoyer mon micro au PC distant » dans Réglages › Audio"))
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
        case "quit":
            if (quitsGame) { closeMenu(); quickMenuManager.quitPlayniteGame(); return }
            openQuitSession()
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
        case "quitSession": openQuitSession(); return
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
    function openKeys() {
        var e = [], macros = KeyMacroManager.macros
        for (var i = 0; i < macros.length; i++) {
            if (macros[i].builtIn) e.push({l: macros[i].name, a: "macro:" + macros[i].id})
        }
        e.push({l: qsTr("Mes macros…"), a: "macros"})
        e.push(cancelEntry)
        openDialog({title: qsTr("Envoyer des touches"), note: qsTr("Envoyées au PC comme si tu les tapais là-bas."), entries: e})
    }
    function openMacros() {
        var e = [], macros = KeyMacroManager.macros
        for (var i = 0; i < macros.length; i++) {
            if (!macros[i].builtIn) e.push({l: macros[i].name, a: "macro:" + macros[i].id})
        }
        if (e.length === 0) {
            openDialog({title: qsTr("Mes macros"), note: qsTr("Aucune macro pour l’instant."),
                        entries: [{l: qsTr("OK"), a: "cancel", main: true}]})
            return
        }
        e.push(cancelEntry)
        openDialog({title: qsTr("Mes macros"), note: qsTr("Envoyées au PC comme si tu les tapais là-bas."), entries: e})
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
    function openMore() {
        var e = [{l: qsTr("Capturer le clavier"), a: "do:capture"},
                 {l: qsTr("Plein écran / fenêtre") + " · " + (quickMenuManager.streamFullScreen() ? qsTr("Plein écran") : qsTr("Fenêtre")), a: "do:fullscreen"}]
        if (SystemProperties.hasTouchScreen) {
            e.push({l: quickMenuManager.trackpadTouchMode() ? qsTr("Passer en multitouch") : qsTr("Passer en trackpad"), a: "do:touch"})
        }
        if (quitsGame) e.push({l: qsTr("Quitter la session"), a: "do:quitSession"})
        e.push(cancelEntry)
        openDialog({title: qsTr("Plus"), note: playingTitle + (via.length ? sep + via : ""), entries: e})
    }
    function openQuitSession() {
        openDialog({title: qsTr("Quitter %1 ?").arg(quitsGame ? quickMenuManager.appTitle : playingTitle), warn: true,
                    note: qsTr("Assure-toi d’avoir sauvegardé : quitter la session ferme l’appli qui tourne sur le PC."),
                    entries: [{l: qsTr("Quitter la session"), a: "quit", main: true}, cancelEntry]})
    }
    function onPicked(a) {
        if (a === "cancel") return
        if (a === "macros") { openMacros(); return }
        if (a.indexOf("do:") === 0) { doEntry(a.substring(3)); return }
        if (a.indexOf("macro:") === 0) { closeMenu(); KeyMacroManager.sendMacro(a.substring(6)); return }
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
                        // the prototype's grid: every tile of a row as tall as the tallest one (min 96)
                        readonly property real need: Math.max(96 * menu.k, 24 * menu.k + 10 * menu.k + bLow.height + 32 * menu.k)
                        onNeedChanged: menu.setNeed(index, need)
                        Component.onCompleted: menu.setNeed(index, need)
                        height: menu.rowHeight(Math.floor(index / 3))
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
                        height: Math.max(54 * menu.k, aLabels.implicitHeight + 12 * menu.k)
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
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
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
        onPicked: (a) => menu.onPicked(a)
        onClosed: menu.forceActiveFocus()
    }
}
