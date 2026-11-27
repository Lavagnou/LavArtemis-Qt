import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtCore

import ".."
import AppModel 1.0
import AppKindModel 1.0
import ComputerModel 1.0
import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0

// Home of the redesigned interface (prototype V8, design-port-spec §1.3-1.5): one host at a time,
// a layout of its own per theme (HomeBraise / HomeAffiche / HomeLudique), the running session staged
// at the top ("Scène"), everything else dimmed and the running item glowing, then Bureau and
// Playnite Fullscreen as two cards, then the 6 most recently played Playnite games and « Tout voir ».
// This file holds the data, the focus grid and the actions; the layouts only draw and call back.
FocusScope {
    id: home
    objectName: "LavArtemis"
    focus: true

    // ---- hosts ---------------------------------------------------------------------------
    property ComputerModel computerModel: createComputerModel()
    // [{name, online, paired, busy, uuid, wakeable, statusUnknown, serverSupported, serverVersion, thisPc,
    //   sessionStartedAt, otherClients, hostDisplay}]
    // A snapshot, rebuilt 50 ms after the model changes: a host is always found again by its uuid
    property var hosts: []
    property int hostPos: 0          // position in `hosts` of the shown host
    readonly property var host: hosts.length > 0 ? hosts[Math.min(hostPos, hosts.length - 1)] : null

    Settings {
        id: uiState
        category: "uiV2"
        property string lastHostUuid: ""
    }

    function createComputerModel() {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', home, '')
        model.initialize(ComputerManager)
        return model
    }

    Instantiator {
        id: hostReader
        model: home.computerModel
        delegate: QtObject {
            required property int index
            required property string name
            required property bool online
            required property bool paired
            required property bool busy
            required property string uuid
            required property bool wakeable
            required property bool statusUnknown
            required property bool serverSupported
            required property string serverVersion
            required property bool thisPc
            required property real sessionStartedAt
            required property int otherClients
            required property string hostDisplay
        }
        onObjectAdded: refreshHostsLater.restart()
        onObjectRemoved: refreshHostsLater.restart()
    }
    Connections {
        target: home.computerModel
        function onDataChanged() { refreshHostsLater.restart() }
        function onModelReset() { refreshHostsLater.restart() }
        // Android's wake toasts: v2_wake_sent / v2_wake_failed
        function onWakeCompleted(uuid, name, sent) {
            home.say(sent ? qsTr("Réveil envoyé à %1").arg(name)
                            : qsTr("Le réveil de %1 n'a pas pu être envoyé.").arg(name))
        }
    }
    Timer {
        id: refreshHostsLater
        interval: 50
        onTriggered: home.refreshHosts()
    }

    // A host to show as soon as it is in the list (« Mes PC » just paired it: the list catches up
    // at the next refresh), like Android's HomeActivity with PREF_LAST_HOST
    property string pendingHostUuid: ""

    function refreshHosts() {
        var previous = host ? host.uuid : uiState.lastHostUuid
        var list = []
        for (var i = 0; i < hostReader.count; i++) {
            var o = hostReader.objectAt(i)
            if (o && o.paired) {
                list.push({name: o.name, online: o.online, paired: o.paired, busy: o.busy, uuid: o.uuid,
                           wakeable: o.wakeable, statusUnknown: o.statusUnknown,
                           serverSupported: o.serverSupported, serverVersion: o.serverVersion, thisPc: o.thisPc,
                           sessionStartedAt: o.sessionStartedAt, otherClients: o.otherClients, hostDisplay: o.hostDisplay})
            }
        }
        // Online hosts first, keeping the model order otherwise
        list.sort(function(a, b) { return (b.online ? 1 : 0) - (a.online ? 1 : 0) })
        var pos = -1
        for (var j = 0; j < list.length; j++) {
            if (list[j].uuid === pendingHostUuid) { pos = j; pendingHostUuid = ""; break }
        }
        if (pos < 0) {
            pos = 0
            for (var k = 0; k < list.length; k++) {
                if (list[k].uuid === previous) { pos = k; break }
            }
        }
        // A forgotten or unpaired host is no longer in the list: another one, or « Aucun PC appairé »
        hostPos = pos
        hosts = list
        syncApps()
    }

    // The app model follows the shown host: there while it is online, dropped when it goes offline,
    // loses its pairing or is forgotten, rebuilt for another host. Keyed by uuid, never by
    // position: hosts move in the list (one comes online, one is forgotten or paired).
    function syncApps() {
        var want = host && host.online ? host.uuid : ""
        var have = appModel ? appModel.hostUuid() : ""
        if (want !== have || (appModel !== null) !== (want !== "")) reloadApps()
    }

    // Row of a host in computerModel, -1 if it left
    function hostRow(uuid) {
        for (var i = 0; i < hostReader.count; i++) {
            var o = hostReader.objectAt(i)
            if (o && o.uuid === uuid) return o.index
        }
        return -1
    }

    // Shows the host with this uuid (from "Mes PC")
    function showHost(uuid) {
        uiState.lastHostUuid = uuid
        for (var j = 0; j < hosts.length; j++) {
            if (hosts[j].uuid === uuid) { pendingHostUuid = ""; hostPos = j; syncApps(); return }
        }
        pendingHostUuid = uuid
        refreshHostsLater.restart()
    }

    function switchHost(delta) {
        if (hosts.length < 2) return
        pendingHostUuid = ""
        hostPos = (hostPos + delta + hosts.length) % hosts.length
        uiState.lastHostUuid = host.uuid
        syncApps()
    }

    // ---- apps of the shown host -----------------------------------------------------------
    property AppModel appModel: null
    property int rev: 0
    // Games hidden with "Masquer": shown (faded) only on demand
    property bool showHidden: false
    readonly property int hiddenCount: { rev; return appModel ? appModel.hiddenAppCount() : 0 }
    readonly property int runningId: { rev; return appModel ? appModel.getRunningAppId() : 0 }
    readonly property int runningRow: { rev; return appModel ? appModel.getRunningAppIndex() : -1 }
    readonly property string runningName: { rev; return appModel && runningId !== 0 ? (appModel.getRunningAppName() || "") : "" }
    readonly property string playniteGame: { rev; return appModel && runningId !== 0 ? appModel.getRunningPlayniteGameName() : "" }
    readonly property bool sessionRunning: runningId !== 0
    readonly property string runningKind: { rev; return appModel && runningId !== 0 ? appModel.getRunningAppKind() : "" }
    // The cards' names, not the host's app names ("Desktop" is shown as Bureau)
    readonly property string runningTitle: runningKind === "desktop" ? qsTr("Bureau")
                                         : (runningKind === "playnite" ? qsTr("Playnite Fullscreen") : runningName)
    // Online, but the host's app list has not arrived yet (Android: « Chargement des applis… »)
    readonly property bool appsLoading: { rev; return appModel !== null && appModel.rowCount() === 0 && hiddenCount === 0 }

    // ---- how long the session has been running (Android's SessionClock) ------------------------
    // LavApollo says when the running app started (ComputerModel sessionStartedAt). An older host
    // does not: the duration then counts from the first time this PC saw that app running on that
    // host, kept across restarts ("<runningId>:<ms>" per host uuid), forgotten when nothing runs.
    // Under a minute nothing is shown.
    Settings {
        id: sessionClockStore
        category: "uiV2SessionClock"
    }
    property real sessionSince: 0
    property real nowMs: Date.now()
    readonly property int sessionMinutes: sessionSince > 0 && nowMs - sessionSince >= 60000
                                          ? Math.floor((nowMs - sessionSince) / 60000) : -1

    function observeSessionClock() {
        nowMs = Date.now()
        // Only with the app list of this very host in hand: an offline host says nothing about its
        // session, and while the shown host changes the app model may still be the previous
        // host's (its running app was written under the new host's uuid)
        if (!host || !host.online || !appModel || appModel.hostUuid() !== host.uuid) {
            sessionSince = 0
            return
        }
        var stored = String(sessionClockStore.value(host.uuid, "") || "")
        if (runningId === 0) {
            if (stored !== "") sessionClockStore.setValue(host.uuid, "")
            sessionSince = 0
            return
        }
        if (host.sessionStartedAt > 0) {
            sessionSince = host.sessionStartedAt
            return
        }
        var since = 0
        var colon = stored.indexOf(":")
        if (colon > 0 && parseInt(stored.substring(0, colon)) === runningId) {
            since = parseFloat(stored.substring(colon + 1)) || 0
        }
        if (since <= 0) {
            since = nowMs
            sessionClockStore.setValue(host.uuid, runningId + ":" + since)
        }
        sessionSince = since
    }
    onRunningIdChanged: observeSessionClock()
    onAppModelChanged: observeSessionClock()
    onHostChanged: observeSessionClock()
    Timer {
        interval: 20000
        repeat: true
        running: home.sessionRunning
        onTriggered: home.nowMs = Date.now()
    }

    // « EN COURS · DANS PLAYNITE FULLSCREEN · DEPUIS 12 MIN » (Android's HomeScreen line)
    readonly property string liveLine: {
        var line = qsTr("EN COURS")
        if (playniteGame.length > 0) line += " · " + qsTr("dans %1").arg(runningTitle)
        if (sessionMinutes >= 0) {
            line += " · " + (sessionMinutes < 60
                             ? qsTr("depuis %1 min").arg(sessionMinutes)
                             : qsTr("depuis %1 h %2").arg(Math.floor(sessionMinutes / 60))
                                                     .arg(String(sessionMinutes % 60).padStart(2, "0")))
        }
        return line.toUpperCase()
    }

    function reloadApps() {
        if (appModel) {
            appModel.computerLost.disconnect(onComputerLost)
            appModel.destroy()
        }
        appModel = null
        if (!host || !host.online) {
            rev++
            return
        }
        var model = Qt.createQmlObject('import AppModel 1.0; AppModel {}', home, '')
        if (!model.initialize(ComputerManager, host.uuid, showHidden)) {
            // Forgotten meanwhile: the list catches up at the next refresh
            model.destroy()
            rev++
            refreshHostsLater.restart()
            return
        }
        model.computerLost.connect(onComputerLost)
        appModel = model
        rev++
    }

    // The host went offline, lost its pairing or was forgotten: its cards go now, and the next
    // refresh (the host list has changed too) shows what is left. Recreating the model here, from
    // the snapshot that still said « online », kept the cards beside « hors ligne ».
    function onComputerLost() {
        if (appModel) {
            appModel.computerLost.disconnect(onComputerLost)
            appModel.destroy()
        }
        appModel = null
        rev++
        refreshHostsLater.restart()
    }

    Connections {
        target: home.appModel
        ignoreUnknownSignals: true
        function onDataChanged() { home.rev++ }
        function onModelReset() { home.rev++ }
        function onRowsInserted() { home.rev++ }
        function onRowsRemoved() { home.rev++ }
        // A game played on the host moves to the front of the library (host order)
        function onRowsMoved() { home.rev++ }
        function onRunningAppChanged() { home.rev++ }
    }

    // Host polling is driven by main.qml (window active/visible), as for the legacy views.
    StackView.onActivated: {
        // The keys come back here: a page pushed by createObject() is not destroyed by the pop and
        // kept the focus of the StackView (no OS focus in the test client: nothing else moves it)
        home.focus = false
        home.focus = true
        home.forceActiveFocus()
        refreshHostsLater.restart()
        rev++
        if (Theme.input === "mouse" && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            Theme.input = "pad"
        }
    }

    Connections {
        target: SdlGamepadKeyNavigation
        function onGamepadInput() { Theme.input = "pad" }
    }

    AppKindModel { id: desktopModel; sourceModel: home.appModel; kind: "desktop" }
    AppKindModel { id: playniteModel; sourceModel: home.appModel; kind: "playnite" }
    AppKindModel { id: gamesModel; sourceModel: home.appModel; kind: "game" }

    // ---- what the layouts show -----------------------------------------------------------------
    // Prototype px are multiplied by k (the home is laid out on the 1600-wide Stage: x 1.25)
    readonly property real k: 1.25
    // The layouts have a `home` property of their own: they are given this one by another name
    readonly property var homeRef: home

    // Snapshot of the apps of the shown host: cards (Bureau, Playnite Fullscreen) and games in the
    // host's order (most recently played first, NvComputer::sortApps), rebuilt on every model change
    Instantiator {
        id: appReader
        model: home.appModel
        delegate: QtObject {
            required property int index
            required property string name
            required property bool running
            required property string boxart
            required property bool hidden
            required property int appid
            required property string kind
            required property bool installed
            required property real lastActivity
            required property real playtime
            required property string background
        }
        onObjectAdded: home.rev++
        onObjectRemoved: home.rev++
    }

    // In the model's order, read from each delegate's `index`: a moved row (a game played on the host
    // goes to the front) bumps `rev` from onRowsMoved before the Instantiator has reordered its objects,
    // and a snapshot by objectAt(i) then kept the old order for good. Reading `index` also re-evaluates
    // this binding when the indexes change.
    readonly property var apps: {
        rev
        var objs = []
        for (var i = 0; i < appReader.count; i++) {
            var o = appReader.objectAt(i)
            if (o) objs.push(o)
        }
        objs.sort(function(a, b) { return a.index - b.index })
        return objs.map(function(o) {
            return {name: o.name, running: o.running, cover: o.boxart, hidden: o.hidden, appid: o.appid,
                    kind: o.kind, installed: o.installed, lastActivity: o.lastActivity,
                    playtime: o.playtime, bg: o.background}
        })
    }
    function appOfKind(kind) {
        for (var i = 0; i < apps.length; i++) if (apps[i].kind === kind && !apps[i].hidden) return apps[i]
        return null
    }
    readonly property var games: apps.filter(function(a) { return a.kind === "game" && !a.hidden })
    readonly property var recents: games.slice(0, 6)
    readonly property bool noGames: appModel !== null && !appsLoading && games.length === 0
    readonly property string libTotal: games.length === 1 ? qsTr("1 jeu") : qsTr("%1 jeux").arg(games.length)

    function hoursText(g) {
        // « 25 min jouées » under an hour (decision of 2026-11-26), « 3 h jouées », « Jamais lancé »
        return SystemProperties.playtimeText(g ? g.playtime : 0)
    }
    // « joué il y a 3 jours » (prototype G[].last): lastActivity in ms on this PC's clock
    function lastPlayedText(ms) {
        var mins = (Date.now() - ms) / 60000
        if (mins < 60) return qsTr("à l’instant")
        var days = Math.floor(mins / 1440)
        if (days < 1) return qsTr("aujourd’hui")
        if (days < 2) return qsTr("hier")
        if (days < 7) return qsTr("il y a %1 jours").arg(days)
        if (days < 14) return qsTr("la semaine dernière")
        if (days < 28) return qsTr("il y a %1 semaines").arg(Math.floor(days / 7))
        if (days < 60) return qsTr("le mois dernier")
        if (days < 365) return qsTr("il y a %1 mois").arg(Math.floor(days / 30))
        if (days < 730) return qsTr("l’an dernier")
        return qsTr("il y a %1 ans").arg(Math.floor(days / 365))
    }

    // The running Playnite game, when there is one (inside Playnite Fullscreen or alone)
    readonly property var runningGame: {
        if (runningKind === "game") {
            for (var i = 0; i < apps.length; i++) if (apps[i].appid === runningId) return apps[i]
        }
        if (playniteGame.length > 0) {
            for (var j = 0; j < apps.length; j++) if (apps[j].kind === "game" && apps[j].name === playniteGame) return apps[j]
            return {name: playniteGame, cover: "", bg: "", appid: 0}
        }
        return null
    }
    // Choice 23: « Reprendre » when no other device is connected to the host, « Rejoindre » when one
    // is (LavApollo OtherClientsConnected; a host that does not say it: Reprendre).
    readonly property bool otherDevice: host !== null && host.otherClients > 0
    readonly property string resumeLabel: otherDevice ? qsTr("Rejoindre") : qsTr("Reprendre")
    function playLabel(isGame) { return isGame ? qsTr("Jouer") : qsTr("Lancer") }

    readonly property bool online: host !== null && host.online && appModel !== null && !appsLoading
    readonly property bool stageOn: online && sessionRunning
    readonly property string stageTitle: runningGame ? runningGame.name : (runningKind === "playnite" ? qsTr("Playnite Fullscreen") : runningTitle)
    // « Bureau Windows » / « Playnite Fullscreen » / « dans Playnite Fullscreen » / « Jeu Playnite »
    readonly property string stageVia: runningKind === "game" ? qsTr("Jeu Playnite")
                                     : runningKind === "playnite" ? (playniteGame.length > 0 ? qsTr("dans Playnite Fullscreen") : qsTr("Playnite Fullscreen"))
                                     : qsTr("Bureau Windows")
    readonly property string stageTime: sessionMinutes < 1 ? qsTr("à l’instant")
                                      : (sessionMinutes < 60 ? qsTr("depuis %1 min").arg(sessionMinutes)
                                         : qsTr("depuis %1 h %2").arg(Math.floor(sessionMinutes / 60)).arg(String(sessionMinutes % 60).padStart(2, "0")))

    // Bureau then Playnite Fullscreen (prototype a0, a1)
    readonly property var cards: {
        var out = []
        var defs = [{kind: "desktop", title: qsTr("Bureau"), sub: qsTr("Ton bureau Windows, tel quel"), num: "01"},
                    {kind: "playnite", title: qsTr("Playnite Fullscreen"), sub: qsTr("Plein écran, choisis ton jeu à la manette"), num: "02"}]
        for (var i = 0; i < defs.length; i++) {
            var a = appOfKind(defs[i].kind)
            var on = a !== null && sessionRunning && runningKind === defs[i].kind
            out.push({id: "a" + i, kind: defs[i].kind, title: defs[i].title, sub: defs[i].sub, num: defs[i].num,
                      appid: a ? a.appid : 0, art: a ? a.cover : "", present: a !== null, running: on,
                      run: on ? ((defs[i].kind === "playnite" && playniteGame.length > 0 ? playniteGame + " · " : "") + stageTime) : "",
                      action: on ? resumeLabel : playLabel(false)})
        }
        return out
    }
    function isLive(g) { return runningGame !== null && g.appid !== 0 && (g.appid === runningGame.appid || g.name === playniteGame) }

    // The item under the focus (a recent game: its name and hours in the section caption)
    readonly property var focusedRecent: /^r\d$/.test(focusId) ? (recents[parseInt(focusId.substring(1))] || null) : null
    readonly property string heroTitle: focusedRecent ? focusedRecent.name + " · " + hoursText(focusedRecent) : ""

    // Braise hero background: the running game (stage), else the focused app card's art (blurred),
    // else the focused recent game, else the running game, else the Bureau's art
    readonly property var hero: {
        var desk = appOfKind("desktop")
        var deskArt = desk ? desk.cover : ""
        var fa = (focusId === "a0" || focusId === "a1") ? cards[parseInt(focusId.substring(1))] : null
        var g = stageOn ? runningGame : null
        if (stageOn) return g ? {src: g.bg || g.cover, blur: !g.bg} : {src: deskArt, blur: false}
        if (fa) return {src: fa.art, blur: true}
        var hg = focusedRecent
        return hg ? {src: hg.bg || hg.cover, blur: !hg.bg} : {src: deskArt, blur: false}
    }

    // Offline / first run / searching / loading: the card that replaces the apps (prototype homeCard)
    readonly property var homeCard: {
        if (!host) return {title: qsTr("AUCUN PC APPAIRÉ"), action: qsTr("Ajouter un PC →"), text: qsTr("Ajoute ton PC pour lancer ton bureau et tes jeux d’ici."), kind: "firstrun"}
        if (!host.online && host.statusUnknown) return {title: qsTr("%1 — RECHERCHE…").arg(host.name), action: qsTr("Connexion à %1…").arg(host.name), text: qsTr("Allume-le ou réveille-le, il apparaîtra ici dès qu’il répond."), kind: "connecting"}
        if (host.online && (appModel === null || appsLoading)) return {title: qsTr("%1 — EN LIGNE").arg(host.name), action: qsTr("Connexion à %1…").arg(host.name), text: qsTr("Chargement des applis…"), kind: "loading"}
        if (!host.online) return host.wakeable ? {title: qsTr("%1 — HORS LIGNE").arg(host.name), action: qsTr("Réveiller →"), text: qsTr("Wake-on-LAN, puis connexion automatique."), kind: "offline"}
                                              : {title: qsTr("%1 — HORS LIGNE").arg(host.name), action: qsTr("Mes PC →"), text: qsTr("Éteint ou injoignable : sans son adresse matérielle (MAC), il ne peut pas être réveillé d’ici."), kind: "nowol"}
        return null
    }
    readonly property string hostStatus: !host ? qsTr("Ajoute ton PC")
                                        : (host.online ? (sessionRunning ? qsTr("Session en cours") : qsTr("En ligne"))
                                           : (host.statusUnknown ? qsTr("Recherche…") : qsTr("Hors ligne")))
    readonly property string hostLine: host ? qsTr("Sur %1").arg(host.name) : ""
    // Choice 3: chevrons with the mouse or touch, LB / RB with the gamepad, Q / E with the keyboard
    readonly property string lbLabel: Theme.input === "pad" ? "LB" : (Theme.input === "keys" ? "Q" : "‹")
    readonly property string rbLabel: Theme.input === "pad" ? "RB" : (Theme.input === "keys" ? "E" : "›")

    // ---- focus grid (prototype grid(), move(), item()) ---------------------------------------------
    property string focusId: "a1"
    readonly property var grid: {
        if (homeCard) return [["nh", "ns"], ["wake"]]
        var rows = [["nh", "ns"]]
        if (stageOn) rows.push(["resume", "squit"])
        rows.push(["a0", "a1"])
        if (!noGames) {
            var r = []
            for (var i = 0; i < recents.length; i++) r.push("r" + i)
            r.push("lib")
            rows.push(r)
        }
        return rows
    }
    // Focus shown: ring at the gamepad / keyboard, the hover look (ringMouse) at the mouse, nothing at touch
    function isFocused(id) { return focusId === id && Theme.input !== "touch" }
    function defaultFocus() { return homeCard ? "wake" : (stageOn ? "resume" : "a1") }
    function ensureFocus() {
        for (var r = 0; r < grid.length; r++) if (grid[r].indexOf(focusId) >= 0) return
        focusId = defaultFocus()
    }
    onGridChanged: ensureFocus()
    onStageOnChanged: if (stageOn && (focusId === "a1" || focusId === "a0")) focusId = "resume"

    // The item drawn for a grid id (HomeTile / HeaderButton `fid`), in whatever layout the theme uses
    function itemOf(id, root) {
        var kids = (root || home).children
        for (var i = 0; i < kids.length; i++) {
            var k = kids[i]
            if (k.fid === id && k.visible) return k
            var inner = itemOf(id, k)
            if (inner) return inner
        }
        return null
    }
    function centreX(id) {
        var it = itemOf(id)
        return it ? it.mapToItem(home, it.width / 2, 0).x : NaN
    }
    // A direction only goes to what is visually that way (audit rule 1): one row only, Up / Down do nothing; a
    // single column, Left / Right do nothing; changing rows goes to the item whose horizontal centre is the closest
    // (measured: two cards above seven covers -- audit A2), not to the same column index
    function move(dx, dy) {
        var g = grid
        var r = -1
        for (var i = 0; i < g.length; i++) if (g[i].indexOf(focusId) >= 0) { r = i; break }
        if (r < 0) { focusId = g[0][0]; return }
        var c = g[r].indexOf(focusId)
        if (dy) {
            var nr = Math.max(0, Math.min(g.length - 1, r + dy))
            if (nr === r) return
            var cx = centreX(focusId), best = -1, bestD = Infinity
            if (!isNaN(cx)) {
                for (var j = 0; j < g[nr].length; j++) {
                    var x = centreX(g[nr][j])
                    if (!isNaN(x) && Math.abs(x - cx) < bestD) { bestD = Math.abs(x - cx); best = j }
                }
            }
            r = nr
            c = best >= 0 ? best : Math.min(c, g[r].length - 1)
        }
        else c = Math.max(0, Math.min(g[r].length - 1, c + dx))
        focusId = g[r][c]
    }
    // Mouse over an item: the focus follows (not within 900 ms of a touch)
    property real touchAt: 0
    function hover(id) {
        if (Date.now() - touchAt < 900) return
        // Only a mouse that moves: Qt also sends hover events when an item moves or appears
        // under a resting pointer (a dialog closes, a card grows), and the keyboard focus was lost
        if (Date.now() - mouseMovedAt > 200) return
        Theme.input = "mouse"
        focusId = id
    }
    property real mouseMovedAt: 0
    property point lastMouse: Qt.point(-1, -1)

    // ---- targets and actions (prototype act(), quick(), options(), ctxOpts()) -----------------------
    // {kind: "app"|"game", appid, title, cover, isGame, installed, hours}
    function targetOf(id) {
        if (id === "a0" || id === "a1") {
            var c = cards[parseInt(id.substring(1))]
            return c && c.present ? {appid: c.appid, title: c.title, cover: c.art, isGame: false, kindName: qsTr("Application"),
                                     sub: c.sub, installed: true, appKind: c.kind} : null
        }
        if (/^r\d$/.test(id)) {
            var g = recents[parseInt(id.substring(1))]
            return g ? gameTarget(g) : null
        }
        // Y on « Reprendre » = the options of what runs (audit A5, as Android's HomeScreen.appFor)
        if (id === "resume" && stageOn) {
            if (runningGame && runningGame.appid) return gameTarget(runningGame)
            return runningKind === "desktop" ? targetOf("a0") : (runningKind === "playnite" ? targetOf("a1") : null)
        }
        return null
    }
    function gameTarget(g) {
        return {appid: g.appid, title: g.name, cover: g.cover, bg: g.bg, isGame: true, kindName: qsTr("Jeu Playnite"),
                installed: g.installed, hours: hoursText(g), appKind: "game"}
    }
    // The target is exactly what runs: the app, or the game (alone or inside Playnite)
    function isRunningTarget(t) {
        if (!sessionRunning || !t) return false
        if (t.isGame) return t.appid === runningId || (playniteGame.length > 0 && t.title === playniteGame)
        return t.appid === runningId
    }
    function inPlaynite(t) { return !!t && t.isGame && sessionRunning && runningKind === "playnite" && !isRunningTarget(t) }
    function closes(t) {
        if (!sessionRunning || isRunningTarget(t)) return false
        return inPlaynite(t) ? playniteGame.length > 0 : true
    }
    // « Le Bureau » / « Playnite Fullscreen » / the game, as the prototype's sessionName()
    readonly property string sessionName: runningKind === "game" ? runningTitle
                                        : runningKind === "playnite" ? qsTr("Playnite Fullscreen") : qsTr("Le Bureau")
    function closingText() {
        if (runningKind === "playnite" && playniteGame.length > 0)
            return qsTr("Playnite Fullscreen et %1 tournent sur %2 : ils seront fermés.").arg(playniteGame).arg(host.name)
        return qsTr("%1 tourne sur %2 : il sera fermé.").arg(sessionName).arg(host.name)
    }

    function act(id) {
        if (id === "nh") return openHosts()
        if (id === "ns") return openSettings()
        // The segue shows what plays (prototype « playing »): the game inside Playnite, its cover and background
        if (id === "resume") return launchTarget({appid: runningId, title: stageTitle, isGame: false,
                                                  cover: runningGame ? runningGame.cover : "", bg: runningGame ? runningGame.bg : ""})
        if (id === "squit") return askQuitApp()
        if (id === "wake") return cardAct()
        if (id === "lib") return openLibrary()
        var t = targetOf(id)
        if (t) quick(t, id)
    }
    // Launch right away, unless it would close what runs: then the options, with the warning
    function quick(t, id) {
        if (closes(t)) return openTargetOptions(t, id, null)
        launchTarget(t)
    }
    function cardAct() {
        var c = homeCard
        if (!c) return
        if (c.kind === "firstrun") return openHosts(true)
        if (c.kind === "connecting" || c.kind === "loading") return say(qsTr("Connexion à %1…").arg(host.name))
        if (c.kind === "nowol") return openHosts()
        wakeHost()
    }
    function openHosts(addFirst) {
        // pushed as a component: the StackView owns the page and destroys it when it is popped (an
        // object made by createObject() stayed alive, invisible, and kept the keyboard focus)
        stackView.push(Qt.createComponent("HostsView.qml"), {"home": home})
    }
    function openSettings() {
        stackView.push(Qt.createComponent("SettingsView2.qml"), {})
    }
    function openLibrary() {
        stackView.push(Qt.createComponent("LibraryView.qml"), {"home": home})
    }
    function shoulder(d) {
        if (hosts.length < 1) return
        switchHost(d)
        focusId = (host && host.online) ? "a1" : "wake"
    }

    // ---- options (centred dialog, or a popover at the pointer with the mouse) ----------------------
    // what: "target" | "relaunch" | "quitApp"; target kept by appid (rows move under a dialog)
    property var ctxTarget: null
    property string ctxKind: ""
    property string ctxRet: ""
    function options(id, item, x, y) {
        var t = targetOf(id)
        if (!t) return false
        var at = null
        if (item && Theme.input === "mouse") {
            var p = item.mapToItem(null, x, y)
            at = p
        }
        openTargetOptions(t, id, at)
        return true
    }
    function openTargetOptions(t, id, at) {
        ctxTarget = t
        ctxKind = "target"
        ctxRet = id
        var running = isRunningTarget(t)
        var art = [{l: qsTr("Créer un raccourci sur le bureau (.art)"), a: "export"}]
        var hide = t.isGame ? [{l: appModel && appModel.isAppHidden(t.appid) ? qsTr("Ne plus masquer") : qsTr("Masquer"), a: "hide"}] : []
        var cancel = [{l: qsTr("Annuler"), a: "cancel"}]
        var entries, note, warn = false, popNote = ""
        if (running) {
            entries = [{l: resumeLabel, a: "launch", main: true, play: true}, {l: qsTr("Relancer la session"), a: "askRelaunch"},
                       {l: qsTr("Quitter"), a: "askQuit"}, {l: qsTr("Afficher les détails"), a: "details"}].concat(art, hide, cancel)
            note = qsTr("%1 tourne déjà : %2 garde l’écran actuel du PC. Relancer le recrée pour cet appareil.").arg(t.title).arg(resumeLabel)
        } else {
            var first = playLabel(t.isGame)
            var popFirst = first
            if (inPlaynite(t)) {
                first = playniteGame.length > 0 ? qsTr("Fermer %1 et jouer dans Playnite").arg(playniteGame) : qsTr("Jouer dans Playnite")
                popFirst = qsTr("Jouer dans Playnite")
                if (playniteGame.length > 0) popNote = qsTr("Ferme %1 · le stream continue").arg(playniteGame)
            } else if (sessionRunning) {
                first = qsTr("Fermer %1 et lancer").arg(runningKind === "desktop" ? qsTr("le Bureau") : sessionName)
                popNote = qsTr("Ferme %1").arg(runningKind === "playnite" && playniteGame.length > 0
                                                 ? qsTr("Playnite Fullscreen et %1").arg(playniteGame)
                                                 : (runningKind === "desktop" ? qsTr("le Bureau") : sessionName))
            }
            warn = closes(t)
            note = warn ? closingText() : ""
            entries = [{l: at && warn ? popFirst : first, a: "launch", main: true, play: true}, {l: qsTr("Afficher les détails"), a: "details"}].concat(art, hide, cancel)
        }
        var cfg = {title: t.title, head: headLine(t.kindName), cover: t.cover, warn: warn, info: false, note: note,
                   popNote: popNote, entries: entries}
        if (at) optionsMenu.showPopover(cfg, at.x, at.y)
        else optionsMenu.showDialog(cfg)
    }
    function headLine(kind) {
        var sep = Theme.affiche ? " — " : " · "
        var where = host ? host.name : ""
        return Theme.affiche ? (kind + sep + where).toUpperCase() : kind + (where ? sep + where : "")
    }
    function askRelaunch(t) {
        ctxKind = "relaunch"
        var who = runningKind === "playnite" && playniteGame.length > 0
                ? qsTr("Playnite Fullscreen et %1 seront fermés sur %2, puis relancés").arg(playniteGame).arg(host.name)
                : qsTr("%1 sera fermé sur %2, puis relancé").arg(t.title).arg(host.name)
        optionsMenu.showDialog({title: t.title, head: headLine(t.kindName), cover: t.cover, warn: true,
            note: who + qsTr(" avec un écran virtuel à la taille de cet appareil (%1). Ce qui n’est pas sauvegardé dans le jeu sera perdu.").arg(screensText()),
            entries: [{l: qsTr("Fermer et relancer la session"), a: "relaunch", main: true, play: true}, {l: qsTr("Annuler"), a: "cancel"}]})
    }
    function askQuitApp() {
        var t = {appid: runningId, title: stageTitle, cover: runningGame ? runningGame.cover : "", isGame: runningGame !== null,
                 kindName: runningGame ? qsTr("Jeu Playnite") : qsTr("Application")}
        ctxTarget = t
        ctxKind = "quitApp"
        ctxRet = "squit"
        var who = runningKind === "playnite" && playniteGame.length > 0
                ? qsTr("Playnite Fullscreen et %1 seront fermés sur %2.").arg(playniteGame).arg(host.name)
                : qsTr("%1 sera fermé sur %2.").arg(runningKind === "game" ? runningTitle : sessionName).arg(host.name)
        optionsMenu.showDialog({title: qsTr("Quitter %1 ?").arg(t.title), head: headLine(t.kindName), cover: t.cover, warn: true,
            note: who + " " + qsTr("Ce qui n’est pas sauvegardé sera perdu."),
            entries: [{l: qsTr("Quitter"), a: "quitApp", main: true}, {l: qsTr("Annuler"), a: "cancel"}]})
    }
    function screensText() {
        return StreamingPreferences.useMultiDisplay && StreamingPreferences.useVirtualDisplay ? qsTr("tous tes écrans") : qsTr("cet écran")
    }
    function onOption(a) {
        var t = ctxTarget
        if (a === "cancel" || !t) return
        if (a === "launch") return launchTarget(t)
        if (a === "askRelaunch") return askRelaunch(t)
        if (a === "askQuit") return askQuitApp()
        if (a === "relaunch") return quitRunning(t.appid, t.title, t, qsTr("Session relancée : %1").arg(screensText()))
        if (a === "quitApp") return quitRunning()
        if (a === "details") return openDetails(t)
        if (a === "export") {
            exportArtDialog.appid = t.appid
            exportArtDialog.currentFile = "file:///" + appModel.suggestedArtFileName(t.appid)
            return exportArtDialog.open()
        }
        if (a === "hide" && appModel) appModel.setAppHidden(t.appid, !appModel.isAppHidden(t.appid))
    }
    function openDetails(t) {
        stackView.push(Qt.createComponent("DetailsView.qml"), {"home": home, "target": t})
    }

    // ---- launching -------------------------------------------------------------------------------
    function launchTarget(t) {
        if (!appModel || !t || !t.appid) return
        if (host && !host.serverSupported) {
            // As Android (V2HostActivity.launch) and Mes PC: a message instead of a launch that fails half-way
            return showError(qsTr("La version du logiciel hôte de %1 n'est pas prise en charge (%2). Mets à jour LavApollo sur ce PC.")
                             .arg(host.name).arg(host.serverVersion))
        }
        if (host && host.thisPc) {
            // Same check as Session::validateLaunch, before the stream screen opens
            return showError(qsTr("Impossible de streamer %1 depuis lui-même : LavApollo tourne sur ce PC. Lance le stream depuis un autre appareil.").arg(host.name))
        }
        var appid = t.appid
        var inside = appModel.startsInsidePlaynite(appid)
        // What runs already (Bureau after « Déconnecter », the game inside Playnite) is resumed:
        // same app id, or the running Playnite game picked from its cover
        var resumes = isRunningTarget(t) || inside
        if (sessionRunning && !resumes) return quitRunning(appid, t.title, t)
        if (isRunningTarget(t) && t.isGame && runningKind === "playnite") appid = runningId
        // By id, hidden apps included: Reprendre on a hidden running game works too
        var session = appModel.createSessionForApp(appid)
        if (!session) return showError(qsTr("%1 n'est plus dans la liste des applis de %2.").arg(t.title).arg(host.name))
        var component = Qt.createComponent("../StreamSegue.qml")
        var props = segueProps(t, isRunningTarget(t)
                               ? (otherDevice ? qsTr("Session rejointe") : qsTr("Session reprise"))
                               : inside ? qsTr("%1 lancé dans Playnite : le stream continue").arg(t.title)
                               : qsTr("Connecté à %1").arg(host.name))
        props.appName = t.title
        props.session = session
        // The prototype's segue: « Reprise sur » / « Connexion à la session de » for what runs,
        // « Lancement sur » for the rest (a game started inside Playnite included)
        props.isResume = isRunningTarget(t)
        stackView.push(component.createObject(stackView, props))
    }
    // The segue's picture and host, the toast said once the stream is on screen, and the way home
    function segueProps(t, toast) {
        return {"hostName": host ? host.name : "", "coverSource": t && t.cover ? t.cover : "",
                "bgSource": t && t.bg ? t.bg : "", "joining": otherDevice, "startToast": toast, "home": home}
    }
    // kept for HostsView / CliPair callers
    function launch(row, name, appid) { launchTarget({appid: appid, title: name, isGame: false}) }

    // thenTarget: what starts once the running app is closed (« Fermer X et lancer », relaunch)
    function quitRunning(thenAppId, thenName, thenTarget, thenToast) {
        if (!appModel) return
        var model = appModel
        var component = Qt.createComponent("../QuitSegue.qml")
        var params = {"appName": stageTitle, "quitRunningAppFn": function() { model.quitRunningApp() },
                      "hostName": host ? host.name : "", "home": home,
                      "coverSource": runningGame && runningGame.cover ? runningGame.cover : "",
                      "bgSource": runningGame && runningGame.bg ? runningGame.bg : ""}
        var nextSession = thenAppId ? appModel.createSessionForApp(thenAppId) : null
        params.nextAppName = nextSession ? thenName : null
        params.nextSession = nextSession
        if (nextSession) {
            params.nextSegueProps = segueProps(thenTarget || null, thenToast || qsTr("Connecté à %1").arg(host ? host.name : ""))
        }
        stackView.push(component.createObject(stackView, params))
    }

    function showError(text) {
        optionsMenu.showDialog({title: qsTr("Oups"), head: headLine("PC"), info: true, note: text,
                                entries: [{l: qsTr("OK"), a: "cancel", main: true}]})
    }
    function say(text) { toast.show(text) }

    // The home's « Réveiller », with the messages of Mes PC (as Android's home)
    function wakeHost() {
        if (!host) return
        if (host.wakeable) {
            // The toast says whether the packets left (computerModel.wakeCompleted), as Android
            var row = hostRow(host.uuid)
            if (row >= 0) computerModel.wakeComputer(row)
        } else {
            showError(qsTr("%1 ne peut pas être réveillé d'ici : il n'a encore jamais donné son adresse réseau matérielle (MAC). Allume-le une fois à la main pendant que LavArtemis tourne.").arg(host.name))
        }
    }

    // ---- header buttons ------------------------------------------------------------------------------
    function cycleMode() {
        Theme.cycleMode()
        say(Theme.mode === 0 ? qsTr("Clair ou sombre : suit le système") : (Theme.mode === 1 ? qsTr("Thème clair") : qsTr("Thème sombre")))
    }
    readonly property string modeTitle: Theme.mode === 0 ? qsTr("Clair ou sombre : suit le système (%1) · clic pour changer").arg(Theme.systemDark ? qsTr("sombre") : qsTr("clair"))
                                      : (Theme.mode === 1 ? qsTr("Thème clair · clic pour changer") : qsTr("Thème sombre · clic pour changer"))
    readonly property bool multiOn: StreamingPreferences.useMultiDisplay && StreamingPreferences.useVirtualDisplay
    readonly property string multiTip: !StreamingPreferences.useVirtualDisplay ? qsTr("Nécessite l’écran virtuel")
                                     : (StreamingPreferences.useMultiDisplay ? qsTr("Multi-écran : tous tes écrans") : qsTr("Un seul écran : clic pour streamer sur tous tes écrans"))
    function toggleMulti() {
        if (!StreamingPreferences.useVirtualDisplay)
            return say(qsTr("Le multi-écran a besoin de l’écran virtuel (Réglages › Affichage)"))
        StreamingPreferences.useMultiDisplay = !StreamingPreferences.useMultiDisplay
        StreamingPreferences.save()
        if (sessionRunning) {
            ctxKind = "multi"
            ctxTarget = {appid: runningId, title: stageTitle, kindName: qsTr("Application"), isGame: false}
            optionsMenu.showDialog({title: StreamingPreferences.useMultiDisplay ? qsTr("Multi-écran activé") : qsTr("Multi-écran désactivé"),
                head: headLine("PC"), warn: true,
                note: qsTr("Il s’appliquera au prochain lancement : %1. La session en cours garde ses écrans actuels.").arg(screensText()),
                entries: [{l: qsTr("Relancer la session maintenant"), a: "askRelaunch", main: true}, {l: qsTr("Plus tard"), a: "cancel"}]})
        } else {
            say(StreamingPreferences.useMultiDisplay ? qsTr("Multi-écran activé : tous tes écrans") : qsTr("Multi-écran désactivé : un seul écran"))
        }
    }

    // ---- input: keys, gamepad (sent as keys by SdlGamepadKeyNavigation), mouse ------------------------
    HoverHandler {
        onPointChanged: {
            if (point.device.type !== PointerDevice.Mouse) return
            var p = point.scenePosition
            if (p.x === home.lastMouse.x && p.y === home.lastMouse.y) return
            home.lastMouse = Qt.point(p.x, p.y)
            home.mouseMovedAt = Date.now()
            if (Theme.input !== "mouse" && Date.now() - home.touchAt > 900) Theme.input = "mouse"
        }
    }
    // A held 500 ms on a card, a game or a host = options; released before = activate. The release decides
    // by the time really held, not by the timer alone: Windows hands a timer over only when no other message
    // waits, so after a busy moment a release could come before a timer already due, and a held key launched
    // the game
    property real holdSince: 0
    Timer {
        id: holdTimer
        interval: 500
        onTriggered: home.options(home.focusId, null, 0, 0)
    }
    function holdable(id) { return targetOf(id) !== null }
    Keys.onPressed: (event) => {
        // The first direction after the mouse or touch shows the ring on Reprendre (else Playnite) without moving:
        // the mouse had moved the focus where nobody saw it (audit A4, as Android)
        var fresh = Theme.input === "mouse" || Theme.input === "touch"
        Theme.input = "keys" // a gamepad key is followed by gamepadInput(), which sets "pad"
        var key = event.key
        if (fresh && (key === Qt.Key_Left || key === Qt.Key_Right || key === Qt.Key_Up || key === Qt.Key_Down)) {
            focusId = defaultFocus()
            event.accepted = true
        }
        else if (key === Qt.Key_Left) { move(-1, 0); event.accepted = true }
        else if (key === Qt.Key_Right) { move(1, 0); event.accepted = true }
        else if (key === Qt.Key_Up) { move(0, -1); event.accepted = true }
        else if (key === Qt.Key_Down) { move(0, 1); event.accepted = true }
        else if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Space) {
            event.accepted = true
            if (event.isAutoRepeat) return
            if (holdable(focusId)) { holdSince = Date.now(); holdTimer.restart() }
            else act(focusId)
        }
        else if (key === Qt.Key_Menu || key === Qt.Key_Y) { options(focusId, null, 0, 0); event.accepted = true }
        else if (key === Qt.Key_Q || key === Qt.Key_PageUp) { shoulder(-1); event.accepted = true }
        else if (key === Qt.Key_E || key === Qt.Key_PageDown) { shoulder(1); event.accepted = true }
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

    // ---- view ---------------------------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        Behavior on color { ColorAnimation { duration: Theme.animMs } }
    }

    Stage {
        id: stage
        anchors.fill: parent
        Loader {
            id: layout
            width: parent.width
            height: parent.height
            sourceComponent: Theme.affiche ? afficheLayout : (Theme.ludique ? ludiqueLayout : braiseLayout)
        }
        Component { id: braiseLayout; HomeBraise { home: homeRef } }
        Component { id: afficheLayout; HomeAffiche { home: homeRef } }
        Component { id: ludiqueLayout; HomeLudique { home: homeRef } }
        HintsBar {
            k: home.k
            // a dialog open: its hints (prototype: « A Valider · B Annuler » over the dimmed page)
            hints: home.optionsOpen ? [{k: "A", l: qsTr("Valider"), act: function() { home.pickOptions() }},
                    {k: "B", l: qsTr("Annuler"), act: function() { home.closeOptions() }}]
                 : home.homeCard ? [{k: "A", l: qsTr("Valider"), act: function() { home.act(home.focusId) }},
                                    {k: "LB RB", l: qsTr("Changer de PC"), act: function() { home.shoulder(1) }}]
                                 : [{k: "A", l: qsTr("Lancer"), act: function() { home.act(home.focusId) }},
                                    {k: "Y", l: qsTr("Options (ou maintenir A)"), act: function() { home.options(home.focusId, null, 0, 0) }},
                                    {k: "LB RB", l: qsTr("Changer de PC"), act: function() { home.shoulder(1) }}]
        }
        LavToast { id: toast; k: home.k }
    }

    readonly property bool optionsOpen: optionsMenu.opened
    function pickOptions() { optionsMenu.pick(optionsMenu.focusIndex) }
    function closeOptions() { optionsMenu.close() }
    LavOptions {
        id: optionsMenu
        onPicked: (a) => home.onOption(a)
        // focus back to the page under the dialog (the home, the library, the details)
        returnFocusTo: stackView.currentItem
    }

    FileDialog {
        id: exportArtDialog
        property int appid: 0
        title: qsTr("Créer un raccourci sur le bureau (.art)")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "art"
        nameFilters: [qsTr("Raccourci LavArtemis (*.art)")]
        onAccepted: {
            var error = home.appModel ? home.appModel.exportArtFile(appid, selectedFile)
                                      : qsTr("Ce PC n'est plus dans la liste.")
            if (error) home.showError(error)
            else home.say(qsTr("Raccourci créé sur le bureau : %1").arg(String(selectedFile).split("/").pop()))
        }
    }
}
