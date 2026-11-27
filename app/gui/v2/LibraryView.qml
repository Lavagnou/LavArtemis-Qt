import QtQuick
import QtQuick.Controls
import QtQuick.Effects

import AppModel 1.0
import ComputerManager 1.0
import SystemProperties 1.0

// The Playnite library of the shown PC (prototype V8 « library », planches 22, 23, 60, 62, 21;
// design-port-spec §1.6): filters Tous / Récents / Installés (+ « Masqués · N »), LB / RB (Q / E)
// to change filter, the games in a grid (Braise 6 columns with the picked game aside, Ludique 8
// columns with a bar at the bottom) or a numbered list (Affiche, the picked game aside). A / click
// = play, Y / right click / ⋯ / A held = options (those of the home). B = back to the home, on
// « Tout voir ». Prototype px x k.
FocusScope {
    id: lib
    objectName: "Bibliothèque"
    focus: true

    property var home: null
    readonly property real k: 1.25
    readonly property string hostName: home && home.host ? home.host.name : ""

    // Its own model: with the hidden games (the « Masqués » filter is where they come back)
    property AppModel appModel: null
    property int rev: 0
    Component.onCompleted: {
        if (!home || !home.host) return
        var m = Qt.createQmlObject('import AppModel 1.0; AppModel {}', lib, '')
        if (m.initialize(ComputerManager, home.host.uuid, true)) appModel = m
        else m.destroy()
    }
    Connections {
        target: lib.appModel
        ignoreUnknownSignals: true
        function onDataChanged() { lib.rev++ }
        function onModelReset() { lib.rev++ }
        function onRowsInserted() { lib.rev++ }
        function onRowsRemoved() { lib.rev++ }
        function onRowsMoved() { lib.rev++ }
    }
    Instantiator {
        id: reader
        model: lib.appModel
        delegate: QtObject {
            required property string name
            required property string boxart
            required property bool hidden
            required property int appid
            required property string kind
            required property bool installed
            required property real lastActivity
            required property real playtime
            required property string background
        }
        onObjectAdded: lib.rev++
        onObjectRemoved: lib.rev++
    }
    readonly property var all: {
        rev
        var out = []
        for (var i = 0; i < reader.count; i++) {
            var o = reader.objectAt(i)
            if (o && o.kind === "game") out.push({name: o.name, cover: o.boxart, hidden: o.hidden, appid: o.appid, installed: o.installed,
                                                  lastActivity: o.lastActivity, playtime: o.playtime, bg: o.background})
        }
        return out
    }
    readonly property var visibleGames: all.filter(function(g) { return !g.hidden })
    readonly property int hiddenCount: all.length - visibleGames.length
    readonly property var filters: {
        var f = [qsTr("Tous"), qsTr("Récents"), qsTr("Installés")]
        if (hiddenCount > 0) f.push(qsTr("Masqués · %1").arg(hiddenCount))
        return f
    }
    property int filter: 0
    readonly property var list: {
        if (filter === 1) {
            // last activity first, « Jamais lancé » last (stable: the host's order otherwise)
            var r = visibleGames.map(function(g, i) { return {g: g, i: i} })
            r.sort(function(a, b) { return (b.g.lastActivity - a.g.lastActivity) || (a.i - b.i) })
            return r.map(function(x) { return x.g })
        }
        if (filter === 2) return visibleGames.filter(function(g) { return g.installed })
        if (filter === 3) return all.filter(function(g) { return g.hidden })
        return visibleGames
    }
    readonly property int cols: Theme.affiche ? 1 : (Theme.ludique ? 8 : 6)
    // Affiche at the gamepad / keyboard: a window of 8 rows around the focus (prototype listWindow), the
    // mouse scrolls the whole list
    readonly property bool windowed: Theme.affiche && (Theme.input === "pad" || Theme.input === "keys")
    readonly property int winStart: windowed ? Math.max(0, Math.min(fi - 3, list.length - 8)) : 0
    readonly property var shown: windowed ? list.slice(winStart, winStart + 8) : list
    // Hidden games never count: a filter that lists every visible game says « N jeux »; « Masqués » says
    // « N jeux masqués » (backend/librarycounttext, tests/librarycount). Affiche's header shows the same line
    // in capitals.
    readonly property string countLine: SystemProperties.libraryCountText(hostName, list.length, visibleGames.length, filter === 0, filter === 3)

    // ---- focus ---------------------------------------------------------------------------------------
    property string focusId: "lg0"
    readonly property int fi: /^lg\d+$/.test(focusId) ? parseInt(focusId.substring(2)) : 0
    readonly property var pick: list[Math.min(fi, list.length - 1)] || null
    function isFocused(id) { return focusId === id && Theme.input !== "touch" }
    MouseMoves { id: mouseMoves }
    function hover(id) { if (!mouseMoves.recent()) return; Theme.input = "mouse"; focusId = id }
    function targetOf(id) {
        if (!/^lg\d+$/.test(id) || !home) return null
        var g = list[parseInt(id.substring(2))]
        return g ? home.gameTarget(g) : null
    }
    function act(id) {
        if (id === "lback") return back()
        var t = targetOf(id)
        if (t) home.quick(t, id)
    }
    function options(id, item, x, y) {
        var t = targetOf(id)
        if (!t) return false
        var at = item && Theme.input === "mouse" ? item.mapToItem(null, x, y) : null
        home.openTargetOptions(t, id, at)
        return true
    }
    function back() { home.focusId = home.noGames ? "a1" : "lib"; stackView.pop() }
    function setFilter(i) { filter = (i + filters.length) % filters.length; focusId = list.length ? "lg0" : "lback" }
    function move(dx, dy) {
        if (!list.length) return
        var n = list.length, i = fi
        if (cols === 1) i += dy !== 0 ? dy : dx
        else if (dy !== 0) { var j = i + dy * cols; if (j >= 0 && j < n) i = j; else if (dy > 0 && Math.floor(i / cols) < Math.floor((n - 1) / cols)) i = n - 1 }
        else { var c = i % cols + dx; if (c >= 0 && c < cols && i + dx < n) i += dx }
        focusId = "lg" + Math.max(0, Math.min(n - 1, i))
    }
    // Held 500 ms = options, by the time really held (HomeView: a release may come before a timer already due)
    property real holdSince: 0
    Timer { id: holdTimer; interval: 500; onTriggered: lib.options(lib.focusId, null, 0, 0) }
    Keys.onPressed: (event) => {
        Theme.input = "keys"
        var key = event.key
        if (key === Qt.Key_Left) { move(-1, 0); event.accepted = true }
        else if (key === Qt.Key_Right) { move(1, 0); event.accepted = true }
        else if (key === Qt.Key_Up) { move(0, -1); event.accepted = true }
        else if (key === Qt.Key_Down) { move(0, 1); event.accepted = true }
        else if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Space) {
            event.accepted = true
            if (event.isAutoRepeat) return
            if (targetOf(focusId)) { holdSince = Date.now(); holdTimer.restart() } else act(focusId)
        }
        else if (key === Qt.Key_Menu || key === Qt.Key_Y) { options(focusId, null, 0, 0); event.accepted = true }
        else if (key === Qt.Key_Q || key === Qt.Key_PageUp) { setFilter(filter - 1); event.accepted = true }
        else if (key === Qt.Key_E || key === Qt.Key_PageDown) { setFilter(filter + 1); event.accepted = true }
        else if (key === Qt.Key_Escape || key === Qt.Key_Backspace) { back(); event.accepted = true }
    }
    Keys.onReleased: (event) => {
        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) && !event.isAutoRepeat) {
            if (holdTimer.running) {
                holdTimer.stop()
                if (Date.now() - holdSince >= holdTimer.interval) options(focusId, null, 0, 0)
                else act(focusId)
            }
            event.accepted = true
        }
    }
    onFocusIdChanged: if (/^lg\d+$/.test(focusId)) grid.positionViewAtIndex(fi - winStart, GridView.Contain)
    StackView.onActivated: { lib.focus = false; lib.focus = true; forceActiveFocus() }

    // ---- view ---------------------------------------------------------------------------------------------
    readonly property bool dark: Theme.dark
    function veil(a) { return Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, a) }
    readonly property string pickHelp: Theme.input === "pad" ? qsTr("Se lance par Playnite sur %1. A pour jouer tout de suite, Y ou A maintenu pour les options et les réglages.").arg(hostName)
                                     : Theme.input === "keys" ? qsTr("Se lance par Playnite sur %1. Entrée pour jouer tout de suite, touche Menu ou Entrée maintenue pour les options et les réglages.").arg(hostName)
                                     : Theme.input === "touch" ? qsTr("Se lance par Playnite sur %1. Touche pour jouer tout de suite, appui long ou ⋯ pour les options et les réglages.").arg(hostName)
                                     : qsTr("Se lance par Playnite sur %1. Clic pour jouer tout de suite, clic droit ou ⋯ pour les options et les réglages.").arg(hostName)
    readonly property string pickHelpShort: Theme.input === "pad" ? qsTr("A : jouer tout de suite, par Playnite sur %1. Y ou A maintenu : options et réglages.").arg(hostName)
                                     : Theme.input === "keys" ? qsTr("Entrée : jouer tout de suite, par Playnite sur %1. Touche Menu ou Entrée maintenue : options et réglages.").arg(hostName)
                                     : Theme.input === "touch" ? qsTr("Touche : jouer tout de suite, par Playnite sur %1. Appui long ou ⋯ : options et réglages.").arg(hostName)
                                     : qsTr("Clic : jouer tout de suite, par Playnite sur %1. Clic droit ou ⋯ : options et réglages.").arg(hostName)

    Rectangle { anchors.fill: parent; color: Theme.bg }
    Stage {
        anchors.fill: parent
        // Braise: the picked game's picture behind (.22) under a veil; its cover blurred when it has no
        // background (prototype pick.blur = BLUR: blur 28 px, saturate 1.2, x1.25)
        Item {
            anchors.fill: parent
            visible: Theme.braise
            clip: true
            Image {
                id: pickBack
                anchors.fill: parent
                source: lib.pick ? (lib.pick.bg || lib.pick.cover) : ""
                fillMode: Image.PreserveAspectCrop
                opacity: 0.22
                asynchronous: true
                visible: !Theme.effects
            }
            MultiEffect {
                visible: Theme.effects
                anchors.fill: parent
                source: pickBack
                opacity: 0.22
                readonly property bool blurred: !!lib.pick && !lib.pick.bg
                blurEnabled: blurred
                blurMax: 64
                blur: blurred ? 1.0 : 0
                saturation: blurred ? 0.2 : 0
                scale: blurred ? 1.25 : 1.0
            }
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.bg }
                    GradientStop { position: 0.62; color: lib.veil(0.86) }
                    GradientStop { position: 1.0; color: lib.veil(0.6) }
                }
            }
        }

        // Affiche header: « ← PC · N JEUX », « Playnite » 54, the filters as underlined words
        Item {
            id: afficheHeader
            visible: Theme.affiche
            width: 780 * lib.k
            height: afficheHead.height + (18 + 12) * lib.k
            Column {
                id: afficheHead
                x: 32 * lib.k; y: 18 * lib.k
                width: parent.width - 64 * lib.k
                spacing: 6 * lib.k
                HomeTile {
                    id: afficheBack
                    home: lib
                    fid: "lback"
                    k: lib.k
                    holdable: false
                    width: afficheBackText.implicitWidth
                    height: afficheBackText.implicitHeight + 8 * lib.k
                    Rectangle { anchors.fill: parent; anchors.margins: -4 * lib.k; color: afficheBack.ringFill ? Theme.ringFillColor : "transparent" }
                    Text {
                        id: afficheBackText
                        anchors.verticalCenter: parent.verticalCenter
                        text: "← " + lib.countLine.toUpperCase()
                        color: afficheBack.ringFill ? Theme.ringFillFg : Theme.fg
                        font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 14 * lib.k
                    }
                }
                Item {
                    width: parent.width
                    height: afficheTitle.implicitHeight
                    Text {
                        id: afficheTitle
                        text: "Playnite"
                        color: Theme.fg
                        font.family: Theme.displayFont; font.weight: Font.Black; font.pixelSize: 54 * lib.k
                        font.letterSpacing: -0.05 * 54 * lib.k
                        lineHeight: 0.92
                    }
                    Row {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 6 * lib.k
                        spacing: 18 * lib.k
                        Repeater {
                            model: Theme.affiche ? lib.filters : []
                            delegate: Item {
                                required property string modelData
                                required property int index
                                readonly property bool on: index === lib.filter
                                width: afText.implicitWidth
                                height: afText.implicitHeight + 8 * lib.k
                                Text {
                                    id: afText
                                    y: 2 * lib.k
                                    text: parent.modelData
                                    color: parent.on ? Theme.fg : Theme.muted
                                    font.family: Theme.textFont; font.weight: parent.on ? Font.Black : Font.DemiBold
                                    font.pixelSize: 16 * lib.k
                                }
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 4 * lib.k; color: Theme.fg; visible: parent.on }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: lib.setFilter(parent.index) }
                            }
                        }
                    }
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 3 * lib.k; color: Theme.fg }
        }

        // header: back, title + count, filters
        Item {
            visible: !Theme.affiche
            x: (Theme.ludique ? 48 : 40) * lib.k
            y: (Theme.ludique ? 26 : 24) * lib.k
            width: parent.width - 2 * x
            height: (Theme.ludique ? 56 : 44) * lib.k
            HomeTile {
                id: backBtn
                home: lib
                fid: "lback"
                k: lib.k
                holdable: false
                width: height
                height: parent.height
                radius: Theme.affiche ? 0 : (Theme.ludique ? height / 2 : 14 * lib.k)
                Rectangle { anchors.fill: parent; radius: parent.radius; color: backBtn.ringFill ? Theme.ringFillColor : Theme.surface; border.width: Theme.border * lib.k; border.color: Theme.borderColor }
                LavIcon { anchors.centerIn: parent; name: "chevron-left"; size: 20 * lib.k; color: Theme.fg }
            }
            Column {
                x: backBtn.width + 16 * lib.k
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: Theme.displayText(qsTr("Bibliothèque Playnite"))
                    color: Theme.fg
                    font.family: Theme.displayFont; font.weight: Theme.displayWeight
                    font.pixelSize: (Theme.ludique ? 32 : (Theme.affiche ? 40 : 24)) * lib.k
                    font.letterSpacing: Theme.tracking * 24 * lib.k
                }
                Text { text: lib.countLine; color: Theme.muted; font.family: Theme.textFont; font.weight: Theme.ludique ? Font.DemiBold : Font.Normal; font.pixelSize: (Theme.ludique ? 14 : 13) * lib.k }
            }
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8 * lib.k
                Text {
                    visible: Theme.input === "pad" || Theme.input === "keys"
                    anchors.verticalCenter: parent.verticalCenter
                    text: Theme.input === "pad" ? "LB" : "Q"
                    color: Theme.muted; font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 12 * lib.k
                }
                Repeater {
                    model: lib.filters
                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        readonly property bool on: index === lib.filter
                        height: (Theme.ludique ? 46 : 40) * lib.k
                        width: fText.implicitWidth + (Theme.ludique ? 44 : 36) * lib.k
                        radius: Theme.affiche ? 0 : height / 2
                        color: on ? Theme.fg : (Theme.ludique ? Theme.surface : "transparent")
                        border.width: on || Theme.ludique ? 0 : lib.k
                        border.color: Theme.affiche ? Theme.fg : Theme.off
                        Text {
                            id: fText
                            anchors.centerIn: parent
                            text: parent.modelData
                            color: parent.on ? Theme.bg : (Theme.braise ? Theme.switchKnobOff : Theme.fg)
                            font.family: Theme.textFont
                            font.weight: parent.on ? Font.Bold : (Theme.ludique ? Font.DemiBold : Font.Normal)
                            font.pixelSize: (Theme.ludique ? 15 : 14) * lib.k
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: lib.setFilter(parent.index) }
                    }
                }
                Text {
                    visible: Theme.input === "pad" || Theme.input === "keys"
                    anchors.verticalCenter: parent.verticalCenter
                    text: Theme.input === "pad" ? "RB" : "E"
                    color: Theme.muted; font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 12 * lib.k
                }
            }
        }

        // games
        GridView {
            id: grid
            // The box the games show in (the prototype's grid)...
            readonly property real baseX: (Theme.ludique ? 48 : (Theme.affiche ? 0 : 40)) * lib.k
            readonly property real baseY: Theme.affiche ? afficheHeader.height : (Theme.ludique ? 112 : 100) * lib.k
            readonly property real baseW: (Theme.ludique ? parent.width - 96 * lib.k : 780 * lib.k)
            readonly property real baseH: Theme.affiche ? Math.min(548 * lib.k, parent.height - baseY)
                                                        : parent.height - baseY - (Theme.ludique ? 120 : 64) * lib.k
            // ... and room around it for the focus ring (Ludique 4 + 5 px and x1.04, Affiche's ring2 8 px away): the
            // view clips, a ring must never be cut (user's rule, 2026-11-26). Same cells, same place; Affiche's rows
            // start at the window's left edge, where FocusRing keeps its ring2 inside the window.
            readonly property real ringRoom: 16 * lib.k
            readonly property real roomL: Theme.affiche ? 0 : ringRoom
            x: baseX - roomL
            y: baseY - ringRoom
            width: baseW + roomL + ringRoom
            height: baseH + 2 * ringRoom
            leftMargin: roomL
            rightMargin: ringRoom
            topMargin: ringRoom
            bottomMargin: ringRoom
            clip: true
            model: lib.shown
            boundsBehavior: Flickable.StopAtBounds
            readonly property real gap: (Theme.ludique ? 18 : (Theme.affiche ? 0 : 16)) * lib.k
            cellWidth: baseW / lib.cols
            cellHeight: Theme.affiche ? 56 * lib.k : (Theme.ludique ? 196 : 174) * lib.k + gap
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Item {
                id: cell
                required property var modelData
                required property int index
                width: grid.cellWidth
                height: grid.cellHeight
                HomeTile {
                    id: tile
                    home: lib
                    fid: "lg" + (lib.winStart + cell.index)
                    k: lib.k
                    x: Theme.affiche ? 0 : grid.gap / 2
                    width: parent.width - (Theme.affiche ? 0 : grid.gap)
                    height: Theme.affiche ? parent.height : parent.height - grid.gap
                    radius: (Theme.affiche ? 0 : (Theme.ludique ? 24 : 12)) * lib.k
                    dimmed: lib.filter === 3
                    glow: lib.home && lib.home.isLive(cell.modelData)
                    moreSize: Theme.affiche ? 32 : (Theme.ludique ? 32 : 30)
                    moreRight: Theme.affiche ? 32 : 6
                    moreTop: Theme.affiche ? 12 : 6
                    moreRadius: Theme.affiche ? 0 : moreSize / 2
                    moreDots: 16
                    moreBg: Theme.affiche ? "transparent" : (Theme.ludique ? "#FFFFFF" : lib.veil(0.78))
                    moreFg: Theme.ludique ? "#1B1530" : Theme.fg
                    CoverFace {
                        visible: !Theme.affiche
                        anchors.fill: parent
                        radius: tile.radius
                        source: cell.modelData.cover
                        name: cell.modelData.name
                        bg: Theme.ludique ? Theme.surface2 : Theme.surface
                        k: lib.k
                    }
                    Rectangle {
                        visible: !Theme.affiche && cell.modelData.installed
                        x: (Theme.ludique ? 8 : 6) * lib.k; y: x
                        height: (Theme.ludique ? 24 : 20) * lib.k
                        width: instText.implicitWidth + (Theme.ludique ? 20 : 14) * lib.k
                        radius: height / 2
                        color: Theme.ludique ? "#C6F432" : Theme.bg
                        Text { id: instText; anchors.centerIn: parent; text: qsTr("Installé"); color: Theme.ludique ? "#1B1530" : Theme.accentText; font.family: Theme.textFont; font.weight: Theme.ludique ? Font.ExtraBold : Font.Bold; font.pixelSize: (Theme.ludique ? 12 : 11) * lib.k }
                    }
                    // Affiche: a numbered row
                    Item {
                        visible: Theme.affiche
                        anchors.fill: parent
                        Rectangle { anchors.fill: parent; color: tile.ringFill ? Theme.ringFillColor : "transparent" }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: lib.k; color: Theme.fg }
                        // baseline-aligned like the prototype's row (padding-top 12, gap 18), « ⋯ » at the right
                        Text {
                            id: numText
                            x: 32 * lib.k
                            anchors.baseline: nameText.baseline
                            width: 24 * lib.k
                            text: String(lib.winStart + cell.index + 1).padStart(2, "0")
                            color: tile.ringFill ? Theme.ringFillFg : Theme.fg; font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 13 * lib.k
                        }
                        Text {
                            id: nameText
                            x: numText.x + numText.width + 18 * lib.k
                            y: 12 * lib.k
                            width: hoursText.x - x - 18 * lib.k
                            elide: Text.ElideRight
                            text: cell.modelData.name
                            color: tile.ringFill ? Theme.ringFillFg : Theme.fg; font.family: "Archivo"; font.weight: Font.ExtraBold; font.pixelSize: 27 * lib.k; font.letterSpacing: -0.03 * 27 * lib.k
                        }
                        Text {
                            id: hoursText
                            x: parent.width - (32 + 32 + 18) * lib.k - width
                            anchors.baseline: nameText.baseline
                            text: SystemProperties.playtimeShort(cell.modelData.playtime)
                            color: tile.ringFill ? Theme.ringFillFg : Theme.fg; font.family: "Archivo"; font.weight: Font.DemiBold; font.pixelSize: 13 * lib.k
                        }
                        Rectangle {
                            // the « ⋯ » frame (the dots are the tile's own button)
                            x: parent.width - (32 + 32) * lib.k
                            y: (56 - 32) / 2 * lib.k
                            width: 32 * lib.k; height: width
                            color: "transparent"
                            border.width: 2 * lib.k
                            border.color: tile.ringFill ? Theme.ringFillFg : Theme.fg
                        }
                    }
                }
            }
        }
        Text {
            visible: lib.list.length === 0 && lib.appModel !== null
            x: grid.baseX
            y: grid.baseY + 40 * lib.k
            width: grid.baseW
            wrapMode: Text.WordWrap
            text: lib.all.length === 0 ? qsTr("Aucun jeu Playnite sur ce PC. Installe l’extension LavApollo Link dans Playnite pour voir ta bibliothèque ici.")
                                       : qsTr("Aucun jeu dans ce filtre.")
            color: Theme.muted
            font.family: Theme.textFont
            font.pixelSize: 16 * lib.k
        }

        // the picked game: aside (Braise, Affiche) or a bar at the bottom (Ludique)
        Rectangle {
            visible: !Theme.ludique && lib.pick !== null
            anchors.right: parent.right
            anchors.rightMargin: Theme.affiche ? 0 : 40 * lib.k
            y: Theme.affiche ? 0 : 100 * lib.k
            width: (Theme.affiche ? 500 : 380) * lib.k
            height: Theme.affiche ? parent.height : aside.height
            color: Theme.affiche ? (lib.dark ? "#EDEDE8" : "#111111") : "transparent"
            Column {
                id: aside
                x: Theme.affiche ? 32 * lib.k : 0
                y: Theme.affiche ? 32 * lib.k : 0
                width: parent.width - 2 * x
                spacing: (Theme.affiche ? 18 : 14) * lib.k
                CoverFace {
                    width: Theme.affiche ? 220 * lib.k : parent.width
                    height: Theme.affiche ? 330 * lib.k : 214 * lib.k
                    radius: Theme.affiche ? 0 : 18 * lib.k
                    source: lib.pick ? (Theme.affiche ? lib.pick.cover : (lib.pick.bg || lib.pick.cover)) : ""
                    // Braise's fiche: the cover blurred when the game has no background (pick.blur)
                    blurred: Theme.braise && !!lib.pick && !lib.pick.bg
                    name: lib.pick ? lib.pick.name : ""
                    k: lib.k
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: lib.pick ? lib.pick.name : ""
                    color: Theme.affiche ? (lib.dark ? "#141414" : "#ECECE8") : Theme.fg
                    font.family: Theme.displayFont; font.weight: Theme.displayWeight
                    font.pixelSize: (Theme.affiche ? 40 : 30) * lib.k
                    lineHeight: 1.1
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    readonly property string sep: Theme.affiche ? " — " : " · "
                    text: lib.pick && lib.home ? lib.home.hoursText(lib.pick) + sep
                                                 + (lib.pick.installed ? (Theme.affiche ? qsTr("installé") : qsTr("Installé")) : (Theme.affiche ? qsTr("pas installé") : qsTr("Pas installé")))
                                                 + (lib.pick.lastActivity > 0 ? sep + qsTr("joué %1").arg(lib.home.lastPlayedText(lib.pick.lastActivity)) : "") : ""
                    color: Theme.affiche ? (lib.dark ? "#555550" : "#BDBDB6") : Theme.muted
                    font.family: Theme.textFont; font.pixelSize: 15 * lib.k
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    // Affiche: the short form of the help, under a rule (prototype pickHelpAf)
                    topPadding: Theme.affiche ? 12 * lib.k : 0
                    text: Theme.affiche ? lib.pickHelpShort : lib.pickHelp
                    color: Theme.affiche ? (lib.dark ? "#555550" : "#BDBDB6") : Theme.muted
                    Rectangle { visible: Theme.affiche; width: parent.width; height: lib.k; color: lib.dark ? "#B5B5AE" : "#55554F" }
                    lineHeight: 1.5
                    font.family: Theme.textFont; font.pixelSize: 14 * lib.k
                }
            }
        }
        Rectangle {
            visible: Theme.ludique && lib.pick !== null
            x: 48 * lib.k
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 28 * lib.k
            height: 76 * lib.k
            width: barRow.width + 38 * lib.k
            radius: height / 2
            color: lib.dark ? "#F3EEFF" : "#1B1530"
            Row {
                id: barRow
                x: 10 * lib.k
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14 * lib.k
                CoverFace { width: 40 * lib.k; height: 60 * lib.k; radius: 10 * lib.k; source: lib.pick ? lib.pick.cover : ""; name: ""; k: lib.k }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text { text: lib.pick ? lib.pick.name : ""; color: lib.dark ? "#221C3D" : "#FFFFFF"; font.family: Theme.textFont; font.weight: Font.ExtraBold; font.pixelSize: 19 * lib.k }
                    Text {
                        text: lib.pick && lib.home ? lib.home.hoursText(lib.pick) + " · " + (lib.pick.installed ? qsTr("installé") : qsTr("pas installé"))
                                                     + (lib.pick.lastActivity > 0 ? " · " + qsTr("joué %1").arg(lib.home.lastPlayedText(lib.pick.lastActivity)) : "")
                                                     + " · " + qsTr("via Playnite") : ""
                        color: lib.dark ? "#5A527A" : "#D9D2F5"
                        font.family: Theme.textFont; font.pixelSize: 13 * lib.k
                    }
                }
            }
        }
        HintsBar {
            k: lib.k
            hints: lib.home && lib.home.optionsOpen ? [{k: "A", l: qsTr("Valider"), act: function() { lib.home.pickOptions() }},
                    {k: "B", l: qsTr("Annuler"), act: function() { lib.home.closeOptions() }}]
                 : [{k: "A", l: qsTr("Jouer"), act: function() { lib.act(lib.focusId) }},
                    {k: "Y", l: qsTr("Options"), act: function() { lib.options(lib.focusId, null, 0, 0) }},
                    {k: "LB RB", l: qsTr("Filtre"), act: function() { lib.setFilter(lib.filter + 1) }},
                    {k: "B", l: qsTr("Retour"), act: function() { lib.back() }}]
        }
    }
}
