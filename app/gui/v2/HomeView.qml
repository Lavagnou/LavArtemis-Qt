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

// Home of the redesigned interface: one host at a time. The running session is staged at the top
// ("Scène"), everything else dimmed ("Projecteur") and the running item glows. Then Bureau and
// Playnite Fullscreen as two half cards, then the Playnite library. The host's other apps (Apollo's
// "Virtual Desktop"...) are not shown: the user asked for the home to hold only these two sections.
// Spec: docs/superpowers/specs/2026-11-23-ui-redesign-design.md (LavArtemis).
FocusScope {
    id: home
    objectName: "LavArtemis"
    focus: true

    readonly property bool isV2Page: true

    // ---- hosts ---------------------------------------------------------------------------
    property ComputerModel computerModel: createComputerModel()
    property var hosts: []          // [{index, name, online, paired, busy, uuid}]
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
        }
        onObjectAdded: refreshHostsLater.restart()
        onObjectRemoved: refreshHostsLater.restart()
    }
    Connections {
        target: home.computerModel
        function onDataChanged() { refreshHostsLater.restart() }
        function onModelReset() { refreshHostsLater.restart() }
    }
    Timer {
        id: refreshHostsLater
        interval: 50
        onTriggered: home.refreshHosts()
    }

    function refreshHosts() {
        var previous = host ? host.uuid : uiState.lastHostUuid
        var list = []
        for (var i = 0; i < hostReader.count; i++) {
            var o = hostReader.objectAt(i)
            if (o && o.paired) {
                list.push({index: o.index, name: o.name, online: o.online, paired: o.paired, busy: o.busy, uuid: o.uuid})
            }
        }
        // Online hosts first, keeping the model order otherwise
        list.sort(function(a, b) { return (b.online ? 1 : 0) - (a.online ? 1 : 0) })
        hosts = list
        var pos = 0
        for (var j = 0; j < list.length; j++) {
            if (list[j].uuid === previous) { pos = j; break }
        }
        if (pos !== hostPos || !appModel) {
            hostPos = pos
            reloadApps()
        } else if (host && appModel === null) {
            reloadApps()
        }
    }

    // Shows the host with this uuid (from "Mes PC")
    function showHost(uuid) {
        uiState.lastHostUuid = uuid
        for (var j = 0; j < hosts.length; j++) {
            if (hosts[j].uuid === uuid) { hostPos = j; reloadApps(); return }
        }
        refreshHostsLater.restart()
    }

    function switchHost(delta) {
        if (hosts.length < 2) return
        hostPos = (hostPos + delta + hosts.length) % hosts.length
        uiState.lastHostUuid = host.uuid
        reloadApps()
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
        model.initialize(ComputerManager, host.index, showHidden)
        model.computerLost.connect(onComputerLost)
        appModel = model
        rev++
    }

    function onComputerLost() { reloadApps() }

    Connections {
        target: home.appModel
        ignoreUnknownSignals: true
        function onDataChanged() { home.rev++ }
        function onModelReset() { home.rev++ }
        function onRowsInserted() { home.rev++ }
        function onRowsRemoved() { home.rev++ }
    }

    // Host polling is driven by main.qml (window active/visible), as for the legacy views.
    StackView.onActivated: {
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

    // ---- actions ------------------------------------------------------------------------------
    function launch(row, name, appid) {
        if (!appModel || row < 0) return
        var inside = appModel.startsInsidePlaynite(row)
        if (runningId !== 0 && runningId !== appid && !inside) {
            quitDialog.appName = runningName
            quitDialog.nextAppName = name
            quitDialog.nextRow = row
            quitDialog.open()
            return
        }
        var component = Qt.createComponent("../StreamSegue.qml")
        stackView.push(component.createObject(stackView, {
            "appName": name,
            "session": appModel.createSessionForApp(row),
            "isResume": runningId === appid || inside
        }))
    }

    function quitRunning(thenRow, thenName) {
        var component = Qt.createComponent("../QuitSegue.qml")
        var params = {"appName": runningName, "quitRunningAppFn": function() { appModel.quitRunningApp() }}
        if (thenRow !== undefined && thenRow >= 0) {
            params.nextAppName = thenName
            params.nextSession = appModel.createSessionForApp(thenRow)
        } else {
            params.nextAppName = null
            params.nextSession = null
        }
        stackView.push(component.createObject(stackView, params))
    }

    function openMenu(item, row, name, cover, running, x, y, appid) {
        appMenu.row = row
        appMenu.appid = appid
        appMenu.appName = name
        appMenu.cover = cover
        appMenu.running = running
        appMenu.hidden = appModel ? appModel.isAppHidden(row) : false
        var p = item.mapToItem(home, x, y)
        appMenu.openAt(p.x, p.y)
    }

    function onMenuAction(action) {
        var row = appMenu.row
        if (action === "launch") {
            launch(row, appMenu.appName, appMenu.appid)
        } else if (action === "quit") {
            quitRunning()
        } else if (action === "relaunch") {
            // Quits and starts again, so the host recreates its virtual display for this device
            quitRunning(row, appMenu.appName)
        } else if (action === "art") {
            exportArtDialog.row = row
            exportArtDialog.currentFile = "file:///" + appModel.suggestedArtFileName(row)
            exportArtDialog.open()
        } else if (action === "hide") {
            appModel.setAppHidden(row, true)
        } else if (action === "unhide") {
            appModel.setAppHidden(row, false)
        }
    }

    // ---- input tracking ---------------------------------------------------------------------
    HoverHandler {
        onPointChanged: if (Theme.input !== "mouse" && point.device.type === PointerDevice.Mouse) Theme.input = "mouse"
    }
    Keys.onPressed: (event) => {
        if (Theme.input !== "keys") Theme.input = "keys"
        if (event.key === Qt.Key_Q || event.key === Qt.Key_PageUp) { switchHost(-1); event.accepted = true }
        else if (event.key === Qt.Key_E || event.key === Qt.Key_PageDown) { switchHost(1); event.accepted = true }
        else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down) {
            var next = home.Window.activeFocusItem ? home.Window.activeFocusItem.nextItemInFocusChain(true) : null
            if (next) { next.forceActiveFocus(); event.accepted = true }
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) {
            var prev = home.Window.activeFocusItem ? home.Window.activeFocusItem.nextItemInFocusChain(false) : null
            if (prev) { prev.forceActiveFocus(); event.accepted = true }
        }
    }

    // ---- view -----------------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        Behavior on color { ColorAnimation { duration: Theme.animMs } }
    }

    Stage {
        anchors.fill: parent

        // The header stays put while the page scrolls under it
        Item {
            id: headerHost
            x: 50
            y: 6 // Close under the title bar, which is empty and in the theme's colours (see main.qml themedFrame)
            width: parent.width - 100
            height: headerBar.height
            z: 1

            // Header (prototype V5, piste A, as validated). One row when everything fits: brand
            // and update on the left, host switcher centred, pills on the right. Otherwise a
            // two-row grid: brand | host switcher on the first row, update | pills on the second.
            // Buttons keep their size, only the layout changes (shrinking them was rejected).
            Item {
                id: headerBar
                width: parent.width
                readonly property real updateWidth: updatePill.visible ? updatePill.width + 12 : 0
                // What the single row needs: both side groups plus the switcher at full width.
                // None of these widths depends on twoRows, so this cannot loop.
                readonly property real switcherWidth: 2 * 40 + 2 * 22 + Math.min(hostNameMetrics.width, 240)
                readonly property bool twoRows: brandRow.width + updateWidth + switcherWidth + rightRow.width + 2 * 24 > width
                readonly property real row2: twoRows ? 74 : 0
                height: twoRows ? 2 * 60 + 14 + 14 : 60

                TextMetrics {
                    id: hostNameMetrics
                    font.family: Theme.textFont
                    font.pixelSize: 19
                    font.weight: Font.Bold
                    text: home.host ? home.host.name.toUpperCase() : ""
                }

                Row {
                    id: brandRow
                    y: (60 - height) / 2
                    spacing: 12
                    // The app logo, recoloured for the theme when the original would clash
                    // (LavArtemis docs/branding: Ludique keeps the original night blue).
                    // LavArtemis Dev always shows its own logo, whatever the theme.
                    Image {
                        width: 36; height: 36
                        sourceSize.width: 72; sourceSize.height: 72
                        anchors.verticalCenter: parent.verticalCenter
                        source: SystemProperties.isDevChannel ? "qrc:/res/lavartemis-dev.svg"
                              : Theme.id === "braise" ? "qrc:/res/lavartemis-braise.svg"
                              : Theme.id === "affiche" ? (Theme.dark ? "qrc:/res/lavartemis-affiche-dark.svg"
                                                                     : "qrc:/res/lavartemis-affiche-light.svg")
                              : "qrc:/res/lavartemis.svg"
                    }
                    Text {
                        text: Theme.displayText("LavArtemis")
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.fg
                        font.family: Theme.displayFont
                        font.pixelSize: 26
                        font.weight: Theme.displayWeight
                    }
                    Rectangle {
                        width: devText.implicitWidth + 14; height: 22
                        radius: Theme.id === "affiche" ? 0 : 11
                        color: Theme.badgeBg
                        anchors.verticalCenter: parent.verticalCenter
                        Text { id: devText; anchors.centerIn: parent; text: "DEV"; color: Theme.badgeFg; font.pixelSize: 12; font.bold: true }
                    }
                }

                // The legacy toolbar's update button, which v2 hides (see main.qml updateVersion):
                // after the brand on one row, under it (start of the pills' row) on two.
                LavPill {
                    id: updatePill
                    x: headerBar.twoRows ? 0 : brandRow.width + 12
                    y: headerBar.row2 + (60 - height) / 2
                    iconName: "refresh"
                    text: qsTr("Mise à jour")
                    visible: window.updateVersion !== ""
                    onClicked: window.openUpdateDialog()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("LavArtemis %1 est disponible").arg(window.updateVersion)
                }

                Row {
                    id: centerRow
                    // One row: centred, but never over the side groups. Two rows: at the right
                    // end of the brand's row.
                    x: headerBar.twoRows ? parent.width - width
                       : Math.max(brandRow.width + headerBar.updateWidth + 24,
                                  Math.min((parent.width - width) / 2, rightRow.x - width - 24))
                    y: (60 - height) / 2
                    spacing: 22
                    visible: home.host !== null

                    RoundIconButton {
                        iconName: "chevron-left"
                        enabled: home.hosts.length > 1
                        onClicked: home.switchHost(-1)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: hostNameMetrics.text
                            width: Math.min(implicitWidth, 240)
                            elide: Text.ElideRight
                            color: Theme.fg
                            font.family: Theme.textFont
                            font.pixelSize: 19
                            font.weight: Font.Bold
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: !home.host ? "" : (!home.host.online ? qsTr("Hors ligne")
                                  : (home.sessionRunning ? qsTr("Session en cours") : qsTr("En ligne")))
                            color: Theme.muted
                            font.family: Theme.textFont
                            font.pixelSize: 14
                        }
                    }
                    RoundIconButton {
                        iconName: "chevron-right"
                        enabled: home.hosts.length > 1
                        onClicked: home.switchHost(1)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Row {
                    id: rightRow
                    anchors.right: parent.right
                    y: headerBar.row2 + (60 - height) / 2
                    spacing: 12

                    LavPill {
                        iconName: Theme.dark ? "moon" : "sun"
                        badge: Theme.mode === 0 ? "A" : ""
                        implicitWidth: 50
                        onClicked: Theme.cycleMode()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Clair ou sombre : %1").arg(Theme.modeNames[Theme.mode])
                    }
                    LavPill {
                        iconName: "screens"
                        text: qsTr("Multi-écran")
                        showSwitch: true
                        switchOn: StreamingPreferences.useMultiDisplay
                        visible: StreamingPreferences.useVirtualDisplay
                        onClicked: {
                            StreamingPreferences.useMultiDisplay = !StreamingPreferences.useMultiDisplay
                            StreamingPreferences.save()
                            if (home.sessionRunning) multiDialog.open()
                        }
                    }
                    LavPill {
                        iconName: "monitor"
                        text: qsTr("Mes PC")
                        onClicked: {
                            var component = Qt.createComponent("HostsView.qml")
                            stackView.push(component.createObject(stackView, {"home": home}))
                        }
                    }
                    LavPill {
                        iconName: "gear"
                        text: qsTr("Réglages")
                        onClicked: {
                            var component = Qt.createComponent("SettingsView2.qml")
                            stackView.push(component.createObject(stackView, {}))
                        }
                    }
                }

                // Two rows: a rule under the grid, as on the prototype
                Rectangle {
                    visible: headerBar.twoRows
                    y: parent.height - 1
                    width: parent.width
                    height: 1
                    color: Theme.borderColor
                }
            }
        }

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.topMargin: headerHost.y + headerHost.height
            contentWidth: width
            contentHeight: page.implicitHeight + 60
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: page
                x: 50
                y: 26
                width: flick.width - 100
                spacing: 26

                // Nothing to show: no paired host, or it is offline
                Column {
                    visible: !home.host || !home.host.online
                    width: parent.width
                    spacing: 18
                    topPadding: 80
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: !home.host ? qsTr("Aucun PC appairé") : qsTr("%1 est hors ligne").arg(home.host.name)
                        color: Theme.fg
                        font.family: Theme.displayFont
                        font.pixelSize: 40
                        font.weight: Theme.displayWeight
                    }
                    LavButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: !home.host ? qsTr("Ajouter un PC") : qsTr("Mes PC")
                        iconName: "monitor"
                        onClicked: {
                            var component = Qt.createComponent("HostsView.qml")
                            stackView.push(component.createObject(stackView, {"home": home}))
                        }
                    }
                }

                // Scène: the running session, always on top
                Column {
                    visible: home.sessionRunning
                    width: parent.width
                    spacing: 10

                    Row {
                        spacing: 10
                        Rectangle { width: 8; height: 8; radius: 4; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: qsTr("EN COURS SUR %1").arg(home.host ? home.host.name.toUpperCase() : "")
                            color: Theme.id === "affiche" ? Theme.fg : Theme.accent
                            font.family: Theme.textFont
                            font.pixelSize: 17
                            font.weight: Font.Bold
                            font.letterSpacing: 1
                        }
                    }
                    Text {
                        width: parent.width
                        text: Theme.displayText(home.playniteGame.length > 0 ? home.playniteGame : home.runningTitle)
                        color: Theme.fg
                        elide: Text.ElideRight
                        font.family: Theme.displayFont
                        font.pixelSize: 56
                        font.weight: Theme.displayWeight
                        font.letterSpacing: Theme.tracking * 56
                    }
                    Row {
                        spacing: 18
                        LavButton {
                            id: resumeButton
                            text: qsTr("Reprendre")
                            iconName: "play"
                            hint: Theme.input === "pad" ? "A" : qsTr("Entrée")
                            focus: home.sessionRunning
                            onClicked: home.launch(home.runningRow, home.runningName, home.runningId)
                        }
                        LavButton {
                            primary: false
                            text: qsTr("Quitter")
                            iconName: "stop"
                            onClicked: home.quitRunning()
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: home.playniteGame.length > 0 ? qsTr("dans %1").arg(home.runningTitle) : ""
                            color: Theme.fg
                            font.family: Theme.textFont
                            font.pixelSize: 18
                        }
                    }
                }

                SectionTitle {
                    visible: home.appModel !== null
                    text: home.host ? qsTr("Sur %1").arg(home.host.name) : ""
                }

                // Bureau + Playnite Fullscreen, side by side
                Row {
                    id: mainCards
                    visible: home.appModel !== null && (desktopModel.count + playniteModel.count) > 0
                    width: parent.width
                    spacing: 22
                    readonly property int cards: Math.max(1, desktopModel.count + playniteModel.count)
                    readonly property real cardWidth: (width - spacing * (cards - 1)) / cards

                    Repeater {
                        model: desktopModel
                        delegate: AppCard {
                            width: mainCards.cardWidth
                            title: qsTr("Bureau")
                            subtitle: qsTr("Ton bureau Windows, tel quel")
                            cover: model.boxart
                            running: model.running
                            dimmed: home.sessionRunning && !model.running
                            liveLine: qsTr("EN COURS")
                            onClicked: home.launch(desktopModel.sourceRow(model.index), model.name, model.appid)
                            onMenuRequested: (x, y) => home.openMenu(this, desktopModel.sourceRow(model.index), model.name, model.boxart, model.running, x, y, model.appid)
                            Keys.onMenuPressed: home.openMenu(this, desktopModel.sourceRow(model.index), model.name, model.boxart, model.running, width / 2, height / 2, model.appid)
                        }
                    }
                    Repeater {
                        model: playniteModel
                        delegate: AppCard {
                            width: mainCards.cardWidth
                            title: qsTr("Playnite Fullscreen")
                            subtitle: qsTr("Plein écran, choisis ton jeu à la manette")
                            cover: model.boxart
                            running: model.running
                            dimmed: home.sessionRunning && !model.running
                            liveLine: home.playniteGame.length > 0 ? qsTr("EN COURS · %1").arg(home.playniteGame) : qsTr("EN COURS")
                            onClicked: home.launch(playniteModel.sourceRow(model.index), model.name, model.appid)
                            onMenuRequested: (x, y) => home.openMenu(this, playniteModel.sourceRow(model.index), model.name, model.boxart, model.running, x, y, model.appid)
                            Keys.onMenuPressed: home.openMenu(this, playniteModel.sourceRow(model.index), model.name, model.boxart, model.running, width / 2, height / 2, model.appid)
                        }
                    }
                }

                SectionTitle {
                    visible: home.appModel !== null && gamesModel.count > 0
                    text: qsTr("Bibliothèque Playnite")
                }
                Flow {
                    visible: home.appModel !== null && gamesModel.count > 0
                    width: parent.width
                    spacing: 20
                    Repeater {
                        model: gamesModel
                        delegate: CoverTile {
                            title: model.name
                            cover: model.boxart
                            // The game running inside Playnite glows too (matched by name)
                            running: model.running || (home.playniteGame.length > 0 && model.name === home.playniteGame)
                            dimmed: home.sessionRunning && !model.running && model.name !== home.playniteGame
                            // Shown only with "Afficher les jeux masqués"
                            opacity: model.hidden ? 0.45 : 1.0
                            onClicked: home.launch(gamesModel.sourceRow(model.index), model.name, model.appid)
                            onMenuRequested: (x, y) => home.openMenu(this, gamesModel.sourceRow(model.index), model.name, model.boxart, model.running, x, y, model.appid)
                            Keys.onMenuPressed: home.openMenu(this, gamesModel.sourceRow(model.index), model.name, model.boxart, model.running, width / 2, height / 2, model.appid)
                        }
                    }
                }

                // Hidden games ("Masquer" in the menu): shown again on demand, to unhide them
                LavButton {
                    visible: home.hiddenCount > 0
                    primary: false
                    iconName: home.showHidden ? "eye-off" : "eye"
                    text: home.showHidden ? qsTr("Cacher les jeux masqués")
                                          : (home.hiddenCount > 1 ? qsTr("Afficher les %1 jeux masqués").arg(home.hiddenCount)
                                                                  : qsTr("Afficher le jeu masqué"))
                    onClicked: { home.showHidden = !home.showHidden; home.reloadApps() }
                }
            }
        }
    }

    AppMenu {
        id: appMenu
        property int row: -1
        property int appid: 0
        parent: home
        onTriggered: (action) => home.onMenuAction(action)
    }

    NavigableMessageDialog {
        id: quitDialog
        property string appName: ""
        property string nextAppName: ""
        property int nextRow: -1
        text: qsTr("%1 est en cours. Le quitter pour lancer %2 ? Ce qui n'est pas sauvegardé sera perdu.").arg(appName).arg(nextAppName)
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: home.quitRunning(nextRow, nextAppName)
    }

    NavigableMessageDialog {
        id: multiDialog
        text: qsTr("Le multi-écran s'appliquera à la prochaine session. Relance la session en cours pour l'appliquer maintenant (menu « … » › Relancer la session).")
        standardButtons: Dialog.Ok
    }

    FileDialog {
        id: exportArtDialog
        property int row: -1
        title: qsTr("Créer un raccourci .art")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "art"
        nameFilters: [qsTr("Raccourci LavArtemis (*.art)")]
        onAccepted: {
            var error = home.appModel.exportArtFile(row, selectedFile)
            if (error) {
                artErrorDialog.text = error
                artErrorDialog.open()
            }
        }
    }

    ErrorMessageDialog {
        id: artErrorDialog
    }
}
