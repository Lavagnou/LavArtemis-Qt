import QtQuick
import QtQuick.Controls
import QtQuick.Window

import StreamingPreferences 1.0
import ComputerManager 1.0
import SystemProperties 1.0
import ClipboardManager 1.0
import ArtemisSettings 1.0
import ProfileManager 1.0
import AutoUpdateChecker 1.0
import PyroWaveCalibrator 1.0
import NetworkBuffers 1.0
import VrrDiagnostics 1.0
import SdlGamepadKeyNavigation 1.0

// Réglages of the redesigned interface: pages on the left, rows on the right. Every setting the
// user triaged "Normal" shows; "Expert" rows only in expert mode (ArtemisSettings.uiExpertMode);
// "Supprimer" settings are gone (StreamingPreferences forces them to their defaults). How each
// preference is read and written -- side effects included -- follows the legacy SettingsView.qml
// it replaced.
//
// ArtemisSettings and StreamingPreferences setters do not persist by themselves: every change
// calls persist(), and the page saves again when it leaves the stack.
FocusScope {
    id: settingsPage
    objectName: qsTr("Réglages")
    focus: true


    property string currentPage: "profil"

    readonly property var pageDefs: [
        {id: "profil", title: qsTr("Profil")},
        {id: "affichage", title: qsTr("Affichage")},
        {id: "video", title: qsTr("Vidéo")},
        {id: "audio", title: qsTr("Audio")},
        {id: "entrees", title: qsTr("Entrées")},
        {id: "manettes", title: qsTr("Manettes")},
        {id: "apparence", title: qsTr("Apparence")},
        {id: "pc", title: qsTr("PC")},
        {id: "raccourcis", title: qsTr("Raccourcis clavier")},
        {id: "avance", title: qsTr("Avancé")}
    ]

    function pageColumn(id) {
        switch (id) {
        case "profil": return pageProfil
        case "affichage": return pageAffichage
        case "video": return pageVideo
        case "audio": return pageAudio
        case "entrees": return pageEntrees
        case "manettes": return pageManettes
        case "apparence": return pageApparence
        case "pc": return pagePc
        case "raccourcis": return pageRaccourcis
        case "avance": return pageAvance
        }
        return null
    }

    // Expert rows of a page the platform offers, hidden while expert mode is off (`expertMode` only makes the
    // binding follow the switch)
    function hiddenCount(id, expertMode) {
        if (expertMode) return 0
        var column = pageColumn(id), count = 0
        if (!column) return 0
        for (var i = 0; i < column.children.length; i++) {
            var c = column.children[i]
            if (c.expert === true && c.available !== false) count++
        }
        return count
    }

    function hasRows(column) {
        for (var i = 0; i < column.children.length; i++) {
            if (column.children[i].shown === true) return true
        }
        return false
    }

    // A page shows when one of its rows does (Manettes is all expert, for instance)
    readonly property var visiblePages: {
        var out = []
        for (var i = 0; i < pageDefs.length; i++) {
            var column = pageColumn(pageDefs[i].id)
            if (column && hasRows(column)) out.push(pageDefs[i])
        }
        return out
    }

    readonly property string currentTitle: {
        for (var i = 0; i < pageDefs.length; i++) {
            if (pageDefs[i].id === currentPage) return pageDefs[i].title
        }
        return ""
    }

    onVisiblePagesChanged: {
        for (var i = 0; i < visiblePages.length; i++) {
            if (visiblePages[i].id === currentPage) return
        }
        if (visiblePages.length > 0) currentPage = visiblePages[0].id
    }

    onCurrentPageChanged: {
        rowsFlick.contentY = 0
        if (currentPage === "audio") refreshMicDevices()
    }

    // ---- persistence ------------------------------------------------------------------------
    Timer {
        id: saveTimer
        interval: 400
        onTriggered: settingsPage.saveNow()
    }
    function persist() { saveTimer.restart() }
    function saveNow() {
        saveTimer.stop()
        StreamingPreferences.save()
        ArtemisSettings.save()
    }

    StackView.onActivated: {
        if (Theme.input === "mouse" && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            Theme.input = "pad"
        }
        Qt.callLater(focusNav)
    }
    StackView.onDeactivating: saveNow()
    // No deactivating callback when the user just closes the window
    Component.onDestruction: saveNow()

    Component.onCompleted: {
        detectDisplays()
        refreshMicDevices()
        NetworkBuffers.refresh()
    }

    function goBack() {
        saveNow()
        // A language change can leave the pages under us unusable (see the legacy SettingsView)
        if (typeof window !== "undefined" && window.clearOnBack) {
            window.clearOnBack = false
            stackView.pop(null)
        } else {
            stackView.pop()
        }
    }

    // ---- keyboard and gamepad ---------------------------------------------------------------
    Connections {
        target: SdlGamepadKeyNavigation
        function onGamepadInput() { Theme.input = "pad" }
    }

    HoverHandler {
        onPointChanged: if (Theme.input !== "mouse" && point.device.type === PointerDevice.Mouse) Theme.input = "mouse"
    }

    function within(item, area) {
        while (item) {
            if (item === area) return true
            item = item.parent
        }
        return false
    }

    function isHGroup(item) {
        return item && item.parent && item.parent.hgroup === true
    }

    function firstFocusable(item) {
        for (var i = 0; i < item.children.length; i++) {
            var child = item.children[i]
            if (!child.visible || !child.enabled) continue
            if (child.activeFocusOnTab) return child
            var inner = firstFocusable(child)
            if (inner) return inner
        }
        return null
    }

    function navItemFor(id) {
        for (var i = 0; i < navRepeater.count; i++) {
            var item = navRepeater.itemAt(i)
            if (item && item.pageId === id) return item
        }
        return null
    }

    function pageVisible(id) {
        for (var i = 0; i < visiblePages.length; i++) {
            if (visiblePages[i].id === id) return true
        }
        return false
    }

    // Next visible page button after (delta 1) or before (delta -1) the one at `index`
    function navNeighbour(index, delta) {
        for (var i = index + delta; i >= 0 && i < navRepeater.count; i += delta) {
            var item = navRepeater.itemAt(i)
            if (item && item.visible) return item
        }
        return null
    }

    function focusNav() {
        var item = navItemFor(currentPage)
        if (item) item.forceActiveFocus(Qt.TabFocusReason)
        else expertToggle.forceActiveFocus(Qt.TabFocusReason)
    }

    function focusRows() {
        var column = pageColumn(currentPage)
        var item = column ? firstFocusable(column) : null
        if (item) item.forceActiveFocus(Qt.TabFocusReason)
    }

    function switchPage(delta) {
        var pos = 0
        for (var i = 0; i < visiblePages.length; i++) {
            if (visiblePages[i].id === currentPage) pos = i
        }
        pos = Math.max(0, Math.min(visiblePages.length - 1, pos + delta))
        currentPage = visiblePages[pos].id
    }

    // Up/Down inside the rows: next focusable item of the rows, skipping the other buttons of a
    // button group (theme cards, profile buttons); never leaves the rows
    function moveInRows(forward) {
        var cur = Window.activeFocusItem
        if (!cur || !within(cur, rowsColumn)) return false
        var next = cur
        for (var n = 0; n < 400; n++) {
            next = next.nextItemInFocusChain(forward)
            if (!next || next === cur || !within(next, rowsColumn)) return false
            if (isHGroup(cur) && next.parent === cur.parent) continue
            next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
            return true
        }
        return false
    }

    // Left/Right inside a button group
    function moveInGroup(forward) {
        var cur = Window.activeFocusItem
        if (!isHGroup(cur)) return false
        var next = cur.nextItemInFocusChain(forward)
        if (next && next.parent === cur.parent) {
            next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
            return true
        }
        return false
    }

    Keys.onPressed: (event) => {
        if (Theme.input !== "keys") Theme.input = "keys"
        var cur = Window.activeFocusItem
        var inRows = within(cur, rowsColumn)
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            goBack()
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            switchPage(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            switchPage(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
            if (inRows) {
                moveInRows(event.key === Qt.Key_Down)
            } else if (event.key === Qt.Key_Down) {
                // From the back button
                expertToggle.forceActiveFocus(Qt.TabFocusReason)
            }
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            if (inRows && !moveInGroup(false)) focusNav()
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            if (inRows) moveInGroup(true)
            event.accepted = true
        }
    }

    // Keep the focused row in view
    Window.onActiveFocusItemChanged: {
        var item = Window.activeFocusItem
        if (!item || !within(item, rowsColumn)) return
        var p = item.mapToItem(rowsFlick.contentItem, 0, 0)
        var margin = 40
        if (p.y - margin < rowsFlick.contentY) {
            rowsFlick.contentY = Math.max(0, p.y - margin)
        } else if (p.y + item.height + margin > rowsFlick.contentY + rowsFlick.height) {
            rowsFlick.contentY = Math.max(0, Math.min(p.y + item.height + margin - rowsFlick.height,
                                                      rowsFlick.contentHeight - rowsFlick.height))
        }
    }

    // ---- stream values ----------------------------------------------------------------------
    readonly property bool pyroWave: StreamingPreferences.videoCodecConfig === StreamingPreferences.VCC_FORCE_PYROWAVE

    // PyroWave needs several hundred Mbps, up to multi-gigabit LANs
    function defaultBitrate() {
        if (pyroWave) {
            return StreamingPreferences.getDefaultPyroWaveBitrate(StreamingPreferences.width,
                                                                  StreamingPreferences.height,
                                                                  StreamingPreferences.fps,
                                                                  StreamingPreferences.enableYUV444,
                                                                  StreamingPreferences.enableHdr)
        }
        return StreamingPreferences.getDefaultBitrate(StreamingPreferences.width,
                                                      StreamingPreferences.height,
                                                      StreamingPreferences.fps,
                                                      StreamingPreferences.enableYUV444)
    }
    readonly property int defaultKbps: defaultBitrate()
    // Android's row (SettingsCatalog KEY_BITRATE): 0,5 to 800 Mbit/s (3 Gbit/s with PyroWave) on a
    // cubic track, and « Saisir un débit » for any value past it. A value typed or calibrated above
    // the track is kept: the knob sits at the end.
    readonly property int bitrateMin: 500
    // PyroWave needs hundreds of Mbit/s: its track goes much higher (same two values as Android)
    readonly property int bitrateMax: pyroWave ? 3000000 : 800000

    function mbps(kbps) {
        return String(Math.round(kbps / 100) / 10).replace(".", ",")
    }

    // "Ajustement automatique du débit": a resolution, frame rate, chroma or codec change resets the
    // bitrate to its default until the user moves the slider (same rule as the legacy screen)
    function applyDefaultBitrateIfAuto() {
        if (StreamingPreferences.autoAdjustBitrate) {
            StreamingPreferences.bitrateKbps = defaultBitrate()
        }
    }

    function setResolution(w, h) {
        if (StreamingPreferences.width !== w || StreamingPreferences.height !== h) {
            StreamingPreferences.width = w
            StreamingPreferences.height = h
            applyDefaultBitrateIfAuto()
            persist()
        }
    }

    function setFps(fps) {
        var changed = StreamingPreferences.fps !== fps
        StreamingPreferences.enableFractionalRefreshRate = false
        if (changed) {
            StreamingPreferences.fps = fps
            applyDefaultBitrateIfAuto()
        }
        persist()
    }

    function setCodec(codec) {
        var wasPyroWave = pyroWave
        StreamingPreferences.videoCodecConfig = codec
        // PyroWave's useful bitrates are an order of magnitude above the other codecs', so
        // switching in or out resets a default
        if (pyroWave !== wasPyroWave && StreamingPreferences.autoAdjustBitrate) {
            StreamingPreferences.bitrateKbps = defaultBitrate()
        } else if (wasPyroWave && !pyroWave && StreamingPreferences.bitrateKbps > bitrateMax) {
            // A bitrate chosen for PyroWave (1,5 Gbit/s) is no use to the other codecs: back onto
            // their track (800 Mbit/s), as Android (SettingsStore.setChoice)
            StreamingPreferences.bitrateKbps = bitrateMax
        }
        persist()
    }

    function applyCalibration(option) {
        StreamingPreferences.videoCodecConfig = StreamingPreferences.VCC_FORCE_PYROWAVE
        StreamingPreferences.width = option.width
        StreamingPreferences.height = option.height
        StreamingPreferences.enableYUV444 = option.chroma444
        StreamingPreferences.enableHdr = option.hdr
        StreamingPreferences.bitrateKbps = option.bitrateKbps
        StreamingPreferences.autoAdjustBitrate = false
        saveNow()
    }

    property var nativeResolutions: []   // [{text, w, h}]
    property var displayRates: []

    function detectDisplays() {
        SystemProperties.refreshDisplays()
        var res = []
        var rates = []
        for (var i = 0; i < 16; i++) {
            var screen = SystemProperties.getNativeResolution(i)
            if (screen.width === 0) break
            res.push({text: qsTr("Native"), w: screen.width, h: screen.height})
            // The notch is a macOS thing: elsewhere the safe area is the whole screen
            var safe = SystemProperties.getSafeAreaResolution(i)
            if (Qt.platform.os === "osx" && safe.width > 0) res.push({text: qsTr("Native sans encoche"), w: safe.width, h: safe.height})
        }
        for (var j = 0; j < 16; j++) {
            var rate = SystemProperties.getRefreshRate(j)
            if (rate === 0) break
            rates.push(rate)
        }
        nativeResolutions = res
        displayRates = rates
    }

    readonly property var resolutionOptions: {
        var list = [{text: "720p", w: 1280, h: 720}, {text: "1080p", w: 1920, h: 1080},
                    {text: "1440p", w: 2560, h: 1440}, {text: "4K", w: 3840, h: 2160}]
        function has(w, h) {
            for (var i = 0; i < list.length; i++) {
                if (list[i].w === w && list[i].h === h) return true
            }
            return false
        }
        for (var n = 0; n < nativeResolutions.length; n++) {
            var r = nativeResolutions[n]
            if (!has(r.w, r.h)) list.push({text: r.text + " · " + r.w + " × " + r.h, w: r.w, h: r.h})
        }
        // Resolutions over the decoder's maximum are not offered
        var maxPixels = SystemProperties.maximumResolution.width * SystemProperties.maximumResolution.height
        if (maxPixels > 0) list = list.filter(function(o) { return o.w * o.h <= maxPixels })
        list.sort(function(a, b) { return a.w * a.h - b.w * b.h })
        var cw = StreamingPreferences.width
        var ch = StreamingPreferences.height
        if (!has(cw, ch)) list.push({text: cw + " × " + ch, w: cw, h: ch})
        var out = list.map(function(o) { return {text: o.text, value: o.w + "x" + o.h} })
        out.push({text: qsTr("Personnalisée…"), value: "custom", ask: true})
        return out
    }

    readonly property bool fractionalRate: StreamingPreferences.enableFractionalRefreshRate && StreamingPreferences.customRefreshRate > 0

    readonly property var fpsOptions: {
        var rates = [30, 60]
        for (var i = 0; i < displayRates.length; i++) {
            if (rates.indexOf(displayRates[i]) < 0) rates.push(displayRates[i])
        }
        if (!fractionalRate && rates.indexOf(StreamingPreferences.fps) < 0) rates.push(StreamingPreferences.fps)
        rates.sort(function(a, b) { return a - b })
        var out = rates.map(function(r) { return {text: String(r), value: r} })
        if (fractionalRate) {
            out.push({text: StreamingPreferences.customRefreshRate.toFixed(2).replace(".", ","), value: "current"})
        }
        out.push({text: qsTr("Autre…"), value: "custom", ask: true})
        return out
    }

    readonly property bool vrrForced: StreamingPreferences.enableVsync && StreamingPreferences.enableVrr

    function windowModeName(mode) {
        if (mode === StreamingPreferences.WM_FULLSCREEN) return qsTr("Plein écran")
        if (mode === StreamingPreferences.WM_FULLSCREEN_DESKTOP) return qsTr("Sans bordure")
        return qsTr("Fenêtre")
    }

    readonly property var codecOptions: {
        var list = [{text: qsTr("Auto"), value: StreamingPreferences.VCC_AUTO},
                    {text: "H.264", value: StreamingPreferences.VCC_FORCE_H264},
                    {text: "HEVC", value: StreamingPreferences.VCC_FORCE_HEVC},
                    {text: "AV1", value: StreamingPreferences.VCC_FORCE_AV1}]
        // PyroWave only in expert mode (or when it is already the choice, so it never disappears
        // from under the user)
        if (SystemProperties.hasPyroWave && (ArtemisSettings.uiExpertMode || pyroWave)) {
            list.push({text: "PyroWave", value: StreamingPreferences.VCC_FORCE_PYROWAVE})
        }
        return list
    }

    readonly property var languageOptions: [
        {text: qsTr("Automatique (français)"), value: StreamingPreferences.LANG_AUTO},
        {text: "Deutsch", value: StreamingPreferences.LANG_DE},
        {text: "English", value: StreamingPreferences.LANG_EN},
        {text: "Français", value: StreamingPreferences.LANG_FR},
        {text: "简体中文", value: StreamingPreferences.LANG_ZH_CN},
        {text: "Norsk bokmål", value: StreamingPreferences.LANG_NB_NO},
        {text: "русский", value: StreamingPreferences.LANG_RU},
        {text: "Español", value: StreamingPreferences.LANG_ES},
        {text: "日本語", value: StreamingPreferences.LANG_JA},
        {text: "Tiếng Việt", value: StreamingPreferences.LANG_VI},
        {text: "ภาษาไทย", value: StreamingPreferences.LANG_TH},
        {text: "한국어", value: StreamingPreferences.LANG_KO},
        {text: "Magyar", value: StreamingPreferences.LANG_HU},
        {text: "Nederlands", value: StreamingPreferences.LANG_NL},
        {text: "Svenska", value: StreamingPreferences.LANG_SV},
        {text: "Türkçe", value: StreamingPreferences.LANG_TR},
        {text: "Українська", value: StreamingPreferences.LANG_UK},
        {text: "繁體中文", value: StreamingPreferences.LANG_ZH_TW},
        {text: "Português", value: StreamingPreferences.LANG_PT},
        {text: "Português do Brasil", value: StreamingPreferences.LANG_PT_BR},
        {text: "Ελληνικά", value: StreamingPreferences.LANG_EL},
        {text: "Italiano", value: StreamingPreferences.LANG_IT},
        {text: "हिन्दी", value: StreamingPreferences.LANG_HI},
        {text: "Język polski", value: StreamingPreferences.LANG_PL},
        {text: "Čeština", value: StreamingPreferences.LANG_CS},
        {text: "עברית", value: StreamingPreferences.LANG_HE},
        {text: "کوردیی ناوەندی", value: StreamingPreferences.LANG_CKB},
        {text: "Lietuvių", value: StreamingPreferences.LANG_LT},
        {text: "Eesti", value: StreamingPreferences.LANG_ET},
        {text: "Български", value: StreamingPreferences.LANG_BG},
        {text: "Esperanto", value: StreamingPreferences.LANG_EO},
        {text: "தமிழ்", value: StreamingPreferences.LANG_TA}
    ]
    property string languageNote: ""

    readonly property var profileOptions: {
        var entries = [{text: qsTr("Réglages globaux (aucun profil)"), value: ""}]
        var profiles = ProfileManager.profiles
        for (var i = 0; i < profiles.length; i++) {
            entries.push({text: profiles[i].name, value: profiles[i].id})
        }
        return entries
    }

    // Read again each time the Audio page or the list opens: a microphone plugged in meanwhile shows
    property var micDevices: []
    function refreshMicDevices() { micDevices = ArtemisSettings.microphoneDevices() }
    readonly property var micOptions: {
        var list = [{text: qsTr("Micro par défaut du système"), value: ""}]
        for (var i = 0; i < micDevices.length; i++) list.push({text: micDevices[i], value: micDevices[i]})
        return list
    }

    // ---- updates ----------------------------------------------------------------------------
    property bool updateChecking: false
    property string updateStatus: ""
    // Every outcome lands here, "nothing to do" and "failed" included (see the legacy screen)
    Connections {
        target: AutoUpdateChecker
        // The doubled prefix is not a typo: the signal is itself named onUpdateAvailable
        function onOnUpdateAvailable(newVersion, url) {
            if (!settingsPage.updateChecking) return
            settingsPage.updateChecking = false
            settingsPage.updateStatus = qsTr("La version %1 est disponible.").arg(newVersion)
        }
        function onNoUpdateAvailable(currentVersion) {
            if (!settingsPage.updateChecking) return
            settingsPage.updateChecking = false
            settingsPage.updateStatus = qsTr("LavArtemis est à jour (%1).").arg(currentVersion)
        }
        function onUpdateCheckFailed(error) {
            if (!settingsPage.updateChecking) return
            settingsPage.updateChecking = false
            settingsPage.updateStatus = qsTr("La vérification a échoué : %1").arg(error)
        }
    }

    // ---- PyroWave calibration hosts ---------------------------------------------------------
    property var calibrationHosts: SystemProperties.hasPyroWave ? createHostModel() : null
    property var calibrationHostOptions: []
    property string calibrationHostUuid: ""

    function createHostModel() {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', settingsPage, '')
        model.initialize(ComputerManager)
        return model
    }

    Instantiator {
        id: hostReader
        model: settingsPage.calibrationHosts
        delegate: QtObject {
            required property string name
            required property string uuid
        }
        onObjectAdded: refreshHostsTimer.restart()
        onObjectRemoved: refreshHostsTimer.restart()
    }
    Timer {
        id: refreshHostsTimer
        interval: 50
        onTriggered: {
            var list = []
            for (var i = 0; i < hostReader.count; i++) {
                var o = hostReader.objectAt(i)
                if (o) list.push({text: o.name, value: o.uuid})
            }
            settingsPage.calibrationHostOptions = list
            if (list.length > 0 && settingsPage.calibrationHostUuid === "") settingsPage.calibrationHostUuid = list[0].value
        }
    }

    // ---- layout -----------------------------------------------------------------------------
    Rectangle { anchors.fill: parent; color: Theme.bg }

    Stage {
        id: stage
        anchors.fill: parent

        Item {
            anchors.fill: parent

            PageHeader {
                id: header
                x: 50
                y: 6 // Close under the title bar, which is empty and in the theme's colours (see main.qml themedFrame)
                width: parent.width - 100
                title: qsTr("Réglages")
                onBack: settingsPage.goBack()
            }
            // Prototype V8: « Thème Braise · LavArtemis 0.7.0-dev.7 » on the right of the header
            Text {
                anchors.right: header.right
                anchors.verticalCenter: header.verticalCenter
                text: qsTr("Thème %1 · LavArtemis %2").arg(Theme.themeNames[Theme.id] || "").arg(SystemProperties.versionString)
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 18
            }

            // ---- page list ----
            Flickable {
                id: navFlick
                x: 44
                y: header.y + header.height + 26
                width: 352
                height: parent.height - y - 20
                contentHeight: nav.implicitHeight + 20
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Column {
                    id: nav
                    x: 6
                    y: 6
                    width: navFlick.width - 12
                    // prototype V8: pages 40 px apart by 4 (x 1.25)
                    spacing: 5

                    AbstractButton {
                        id: expertToggle
                        width: nav.width
                        height: 80
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        onClicked: {
                            ArtemisSettings.uiExpertMode = !ArtemisSettings.uiExpertMode
                            ArtemisSettings.save()
                        }
                        Keys.onReturnPressed: clicked()
                        Keys.onEnterPressed: clicked()
                        Keys.onPressed: (event) => {
                            if (Theme.input !== "keys") Theme.input = "keys"
                            if (event.key === Qt.Key_Down) {
                                var first = settingsPage.navNeighbour(-1, 1)
                                if (first) first.forceActiveFocus(Qt.TabFocusReason)
                                event.accepted = true
                            } else if (event.key === Qt.Key_Up) {
                                event.accepted = true
                            } else if (event.key === Qt.Key_Right) {
                                settingsPage.focusRows()
                                event.accepted = true
                            }
                        }

                        background: Rectangle {
                            radius: Theme.r
                            color: Theme.surface
                            border.width: Math.max(1, Theme.border)
                            border.color: expertToggle.hovered && Theme.input === "mouse" ? Qt.alpha(Theme.accent, 0.6) : Theme.borderColor
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: -5
                                radius: parent.radius + 5
                                color: "transparent"
                                border.width: 3
                                border.color: Theme.fg
                                visible: expertToggle.activeFocus && Theme.showFocus
                            }
                        }
                        contentItem: Item {
                            Column {
                                x: 20
                                width: parent.width - 108
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 4
                                Text {
                                    text: qsTr("Mode expert")
                                    color: Theme.fg
                                    font.family: Theme.textFont
                                    font.pixelSize: 20
                                    font.weight: Font.Bold
                                }
                                Text {
                                    width: parent.width
                                    text: qsTr("Affiche les réglages avancés")
                                    color: Theme.muted
                                    wrapMode: Text.Wrap
                                    font.family: Theme.textFont
                                    font.pixelSize: 15
                                }
                            }
                            LavSwitch {
                                anchors.right: parent.right
                                anchors.rightMargin: 20
                                anchors.verticalCenter: parent.verticalCenter
                                on: ArtemisSettings.uiExpertMode
                            }
                        }
                    }

                    Item { width: 1; height: 10 }

                    Repeater {
                        id: navRepeater
                        // Every page, hidden when it has no row to show: delegates are never
                        // recreated, so toggling expert mode does not move the focus
                        model: settingsPage.pageDefs
                        delegate: AbstractButton {
                            id: navItem
                            required property var modelData
                            required property int index
                            readonly property string pageId: modelData.id
                            readonly property bool selected: settingsPage.currentPage === pageId
                            visible: settingsPage.pageVisible(pageId)
                            width: nav.width
                            height: 50
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus
                            onClicked: settingsPage.currentPage = pageId
                            // Moving the focus with the keyboard or the gamepad opens the page
                            onActiveFocusChanged: if (activeFocus && Theme.input !== "mouse") settingsPage.currentPage = pageId
                            Keys.onPressed: (event) => {
                                if (Theme.input !== "keys") Theme.input = "keys"
                                if (event.key === Qt.Key_Down) {
                                    var next = settingsPage.navNeighbour(index, 1)
                                    if (next) next.forceActiveFocus(Qt.TabFocusReason)
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Up) {
                                    var prev = settingsPage.navNeighbour(index, -1)
                                    if (!prev) prev = expertToggle
                                    prev.forceActiveFocus(Qt.BacktabFocusReason)
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Return
                                           || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                                    settingsPage.currentPage = pageId
                                    settingsPage.focusRows()
                                    event.accepted = true
                                } else if (event.key === Qt.Key_Left) {
                                    event.accepted = true
                                }
                            }

                            background: Rectangle {
                                radius: Theme.pill
                                color: navItem.selected ? Theme.accent
                                     : (navItem.hovered && Theme.input === "mouse" ? Theme.surface2 : "transparent")
                                border.width: Theme.id === "affiche" && navItem.selected ? 3 : 0
                                border.color: Theme.fg
                                Behavior on color { ColorAnimation { duration: Theme.animMs } }
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -5
                                    radius: parent.radius + 5
                                    color: "transparent"
                                    border.width: 3
                                    border.color: Theme.fg
                                    visible: navItem.activeFocus && Theme.showFocus
                                }
                            }
                            contentItem: Text {
                                leftPadding: 22
                                verticalAlignment: Text.AlignVCenter
                                text: navItem.modelData.title  // the prototype keeps the page names in their case, Affiche too
                                color: navItem.selected ? Theme.onAccent : Theme.fg
                                font.family: Theme.displayFont
                                font.pixelSize: 20
                                font.weight: Font.Bold
                            }
                        }
                    }
                }
            }

            // ---- rows ----
            Flickable {
                id: rowsFlick
                // 14 px more on the left than the rows need, the rows 14 px further in: the same place on screen,
                // and room for a row's focus ring (Ludique 4 + 5 px x 1.25, Affiche's ring2 8 px away), which the
                // list's clip cut by a pixel or two (user's rule, 2026-11-26: a ring is never cut)
                x: navFlick.x + navFlick.width + 16
                y: navFlick.y
                width: parent.width - x - 40
                height: parent.height - y - 20
                contentHeight: rowsColumn.implicitHeight + 40
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: ScrollBar {}

                Column {
                    id: rowsColumn
                    x: 22
                    y: 6
                    width: rowsFlick.width - 44
                    spacing: 18

                    Text {
                        text: Theme.displayText(settingsPage.currentTitle)
                        color: Theme.fg
                        font.family: Theme.displayFont
                        font.pixelSize: 34
                        font.weight: Theme.displayWeight
                        font.letterSpacing: Theme.tracking * 34
                    }

                    // ================================ Profil ================================
                    Column {
                        id: pageProfil
                        visible: settingsPage.currentPage === "profil"
                        width: parent.width
                        spacing: 12

                        SettingChoice {
                            objectName: "set:activeProfile"
                            label: qsTr("Profil actif")
                            // What stays common: ArtemisSettings, plus ProfileManager::isGlobalKey()
                            description: qsTr("Un jeu de réglages de stream par usage, par exemple à la maison et à distance. Ce qui tient à cet appareil reste commun : langue, apparence, micro, mises à jour.")
                            descriptionLines: 3
                            dropdown: true
                            options: settingsPage.profileOptions
                            value: ProfileManager.activeProfileId
                            onChosen: (v) => {
                                if (v === ProfileManager.activeProfileId) return
                                // Persist what is on screen first, or pending edits would land in the
                                // wrong profile
                                settingsPage.saveNow()
                                ProfileManager.setActiveProfile(v)
                            }
                        }

                        SettingRow {
                            label: qsTr("Gérer les profils")
                            description: qsTr("Un nouveau profil part des réglages en cours.")
                            focusable: false
                            Row {
                                readonly property bool hgroup: true
                                spacing: 12
                                LavButton {
                                    primary: false
                                    text: qsTr("Nouveau")
                                    iconName: "plus"
                                    implicitHeight: 48
                                    onClicked: { profileNameDialog.renamingId = ""; profileNameDialog.open() }
                                    Keys.onReturnPressed: clicked()
                                    Keys.onEnterPressed: clicked()
                                }
                                LavButton {
                                    primary: false
                                    text: qsTr("Renommer")
                                    implicitHeight: 48
                                    enabled: ProfileManager.activeProfileId !== ""
                                    opacity: enabled ? 1.0 : 0.4
                                    onClicked: { profileNameDialog.renamingId = ProfileManager.activeProfileId; profileNameDialog.open() }
                                    Keys.onReturnPressed: clicked()
                                    Keys.onEnterPressed: clicked()
                                }
                                LavButton {
                                    primary: false
                                    text: qsTr("Supprimer")
                                    implicitHeight: 48
                                    enabled: ProfileManager.activeProfileId !== ""
                                    opacity: enabled ? 1.0 : 0.4
                                    onClicked: deleteProfileDialog.open()
                                    Keys.onReturnPressed: clicked()
                                    Keys.onEnterPressed: clicked()
                                }
                            }
                        }
                    }

                    // =============================== Affichage ==============================
                    Column {
                        id: pageAffichage
                        // rows saved in the active profile (« ● »), but those marked appWide
                        readonly property bool profileRows: true
                        visible: settingsPage.currentPage === "affichage"
                        width: parent.width
                        spacing: 12

                        SettingChoice {
                            objectName: "set:resolution"
                            label: qsTr("Résolution")
                            description: qsTr("Taille de l’image demandée au PC. Avec l’écran virtuel, le PC crée un écran à cette taille.")
                            dropdown: true
                            options: settingsPage.resolutionOptions
                            value: StreamingPreferences.width + "x" + StreamingPreferences.height
                            onChosen: (v) => {
                                if (v === "custom") {
                                    customResolutionDialog.open()
                                    return
                                }
                                var parts = v.split("x")
                                settingsPage.setResolution(parseInt(parts[0]), parseInt(parts[1]))
                            }
                        }

                        SettingChoice {
                            objectName: "set:fps"
                            label: qsTr("Images par seconde")
                            description: qsTr("Nombre d’images envoyées chaque seconde.")
                            segmentMax: 6
                            options: settingsPage.fpsOptions
                            value: settingsPage.fractionalRate ? "current" : StreamingPreferences.fps
                            onChosen: (v) => {
                                if (v === "custom") customFpsDialog.open()
                                else if (v !== "current") settingsPage.setFps(v)
                            }
                        }

                        SettingChoice {
                            objectName: "set:windowMode"
                            label: qsTr("Affichage du stream")
                            available: SystemProperties.hasDesktopEnvironment
                            enabled: !SystemProperties.rendererAlwaysFullScreen && !settingsPage.vrrForced
                            description: settingsPage.vrrForced
                                         ? qsTr("La VRR impose « Sans bordure » ; ton choix revient pour les sessions sans VRR.")
                                         : qsTr("La fenêtre sans bordure garde Alt+Tab instantané.")
                                           .arg(settingsPage.windowModeName(StreamingPreferences.recommendedFullScreenMode))
                            options: [
                                {text: qsTr("Plein écran"), value: StreamingPreferences.WM_FULLSCREEN},
                                {text: qsTr("Sans bordure"), value: StreamingPreferences.WM_FULLSCREEN_DESKTOP},
                                {text: qsTr("Fenêtre"), value: StreamingPreferences.WM_WINDOWED}
                            ]
                            // VRR sessions use borderless presentation, but the saved preference is
                            // never overwritten
                            value: settingsPage.vrrForced ? StreamingPreferences.WM_FULLSCREEN_DESKTOP : StreamingPreferences.windowMode
                            onChosen: (v) => { StreamingPreferences.windowMode = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:enableVsync"
                            label: qsTr("Synchronisation verticale")
                            description: qsTr("Évite les déchirures d'image ; la couper réduit un peu la latence.")
                            checked: StreamingPreferences.enableVsync
                            onToggled: (v) => { StreamingPreferences.enableVsync = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:framePacing"
                            label: qsTr("Frame Pacing")
                            description: StreamingPreferences.enableVsync
                                         ? qsTr("Retient les images arrivées trop tôt pour éviter les micro-saccades.")
                                         : qsTr("Nécessite la V-Sync.")
                            enabled: StreamingPreferences.enableVsync
                            checked: StreamingPreferences.enableVsync && StreamingPreferences.framePacing
                            onToggled: (v) => { StreamingPreferences.framePacing = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:enableVrr"
                            label: qsTr("Fréquence variable (VRR, G-Sync / FreeSync)")
                            expert: true
                            // Windows only: elsewhere a request falls back to fixed pacing, and
                            // StreamingPreferences::reload() keeps it off
                            available: Qt.platform.os === "windows"
                            description: !StreamingPreferences.enableVsync ? qsTr("Nécessite la V-Sync.")
                                         : (StreamingPreferences.useVirtualDisplay && StreamingPreferences.useMultiDisplay)
                                           ? qsTr("Ignorée tant que « Un écran virtuel par moniteur » est actif : le stream couvre plusieurs moniteurs.")
                                           : qsTr("Affiche chaque image dès son arrivée, en plein écran sans bordure. L'hôte capture à l'instant près.")
                            enabled: StreamingPreferences.enableVsync
                            checked: StreamingPreferences.enableVrr
                            onToggled: (v) => { StreamingPreferences.enableVrr = v; settingsPage.persist() }
                        }

                        SettingChoice {
                            objectName: "set:vrrLatencyMode"
                            label: qsTr("Marge du VRR")
                            expert: true
                            available: StreamingPreferences.enableVrr
                            enabled: StreamingPreferences.enableVsync && StreamingPreferences.enableVrr
                            description: (StreamingPreferences.vrrLatencyMode === StreamingPreferences.VLM_LOW_LATENCY
                                          ? qsTr("Délai minimal ; plus de saccades si le réseau est irrégulier (jusqu'à 1 image de réserve).")
                                          : StreamingPreferences.vrrLatencyMode === StreamingPreferences.VLM_SMOOTH
                                            ? qsTr("Mouvement plus régulier, un peu plus de latence (jusqu'à 4 images de réserve).")
                                            : qsTr("Compromis entre fluidité et latence (jusqu'à 2 images de réserve)."))
                                         + " " + qsTr("Effet à la prochaine connexion.")
                            options: [
                                {text: qsTr("Faible latence"), value: StreamingPreferences.VLM_LOW_LATENCY},
                                {text: qsTr("Équilibrée"), value: StreamingPreferences.VLM_BALANCED_TARGET},
                                {text: qsTr("Fluide"), value: StreamingPreferences.VLM_SMOOTH}
                            ]
                            value: StreamingPreferences.vrrLatencyMode
                            onChosen: (v) => { StreamingPreferences.vrrLatencyMode = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:smoothVrrFrameTiming"
                            label: qsTr("Lisser la cadence (VRR)")
                            expert: true
                            available: StreamingPreferences.enableVrr
                            enabled: StreamingPreferences.enableVsync && StreamingPreferences.enableVrr
                            description: qsTr("Régularise l'affichage quand les FPS du jeu ne collent pas à l'écran du PC distant. Effet à la prochaine connexion.")
                            checked: StreamingPreferences.smoothVrrFrameTiming
                            onToggled: (v) => { StreamingPreferences.smoothVrrFrameTiming = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:useVirtualDisplay"
                            label: qsTr("Écran virtuel")
                            description: qsTr("Le PC distant crée un écran à la taille de celui-ci (Apollo / LavApollo).")
                            checked: StreamingPreferences.useVirtualDisplay
                            onToggled: (v) => { StreamingPreferences.useVirtualDisplay = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:useMultiDisplay"
                            label: qsTr("Un écran virtuel par moniteur")
                            description: StreamingPreferences.useVirtualDisplay
                                         ? qsTr("Recrée tous tes moniteurs sur le PC distant, chacun réaffiché sur le sien.")
                                         : qsTr("Nécessite l'écran virtuel.")
                            enabled: StreamingPreferences.useVirtualDisplay
                            // Bound to the preference alone: greyed out, the choice is kept
                            checked: StreamingPreferences.useMultiDisplay
                            onToggled: (v) => { StreamingPreferences.useMultiDisplay = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:keepAwake"
                            label: qsTr("Garder l'écran allumé pendant le stream")
                            appWide: true
                            expert: true
                            description: qsTr("Empêche la veille et l'économiseur d'écran de ce PC pendant le stream.")
                            checked: StreamingPreferences.keepAwake
                            onToggled: (v) => { StreamingPreferences.keepAwake = v; settingsPage.persist() }
                        }
                    }

                    // ================================= Vidéo ================================
                    Column {
                        id: pageVideo
                        // rows saved in the active profile (« ● »), but those marked appWide
                        readonly property bool profileRows: true
                        visible: settingsPage.currentPage === "video"
                        width: parent.width
                        spacing: 12

                        SettingSlider {
                            objectName: "set:bitrateKbps"
                            label: qsTr("Débit vidéo")
                            description: qsTr("Suit la résolution et les images/s tant que tu ne le changes pas.")
                            from: settingsPage.bitrateMin
                            to: settingsPage.bitrateMax
                            stepSize: 500
                            keyStep: 1000
                            curve: 3
                            value: StreamingPreferences.bitrateKbps
                            valueText: qsTr("%1 Mbit/s").arg(settingsPage.mbps(StreamingPreferences.bitrateKbps))
                                       + (StreamingPreferences.autoAdjustBitrate ? " · " + qsTr("auto") : "")
                            onMoved: (v) => {
                                StreamingPreferences.bitrateKbps = v
                                StreamingPreferences.autoAdjustBitrate = false
                                settingsPage.persist()
                            }
                        }

                        SettingAction {
                            label: qsTr("Revenir au débit recommandé")
                            appWide: true
                            available: StreamingPreferences.bitrateKbps !== settingsPage.defaultKbps
                            description: qsTr("%1 Mbit/s pour cette résolution et ces images par seconde, ajusté ensuite automatiquement.")
                                         .arg(settingsPage.mbps(settingsPage.defaultKbps))
                            buttonText: qsTr("Rétablir")
                            onTriggered: {
                                StreamingPreferences.bitrateKbps = settingsPage.defaultBitrate()
                                StreamingPreferences.autoAdjustBitrate = true
                                settingsPage.persist()
                            }
                        }

                        // Android's « Saisir un débit » (edit_diy_bitrate): any value, past the track's end
                        SettingAction {
                            label: qsTr("Saisir un débit")
                            appWide: true
                            expert: true
                            description: qsTr("Une valeur précise, même au-delà du curseur.")
                            buttonText: qsTr("Saisir…")
                            onTriggered: customBitrateDialog.open()
                        }

                        SettingChoice {
                            objectName: "set:videoCodecConfig"
                            label: qsTr("Format vidéo (codec)")
                            description: settingsPage.pyroWave
                                         ? qsTr("PyroWave : réseau filaire avec des centaines de Mbit/s et un hôte LavApollo, sinon H.264.")
                                         : qsTr("« Auto » choisit le meilleur format que ce PC sait décoder.")
                            segmentMax: 5
                            options: settingsPage.codecOptions
                            value: StreamingPreferences.videoCodecConfig
                            onChosen: (v) => settingsPage.setCodec(v)
                        }

                        SettingRow {
                            label: qsTr("Agrandir le tampon réseau (PyroWave)")
                            appWide: true
                            expert: true
                            available: settingsPage.pyroWave && (Qt.platform.os === "linux" || Qt.platform.os === "windows")
                                       && (NetworkBuffers.needsFix || NetworkBuffers.message !== "")
                            focusable: false
                            descriptionLines: 5
                            description: {
                                var parts = []
                                if (NetworkBuffers.needsFix) parts.push(NetworkBuffers.problemText)
                                if (NetworkBuffers.needsFix && !NetworkBuffers.canApply) parts.push(NetworkBuffers.manualHint + " " + NetworkBuffers.manualCommand)
                                if (NetworkBuffers.message !== "") parts.push(NetworkBuffers.message)
                                return parts.join("\n")
                            }
                            Row {
                                readonly property bool hgroup: true
                                spacing: 12
                                visible: NetworkBuffers.needsFix
                                LavButton {
                                    text: qsTr("Corriger")
                                    implicitHeight: 48
                                    visible: NetworkBuffers.canApply
                                    enabled: !NetworkBuffers.busy
                                    opacity: enabled ? 1.0 : 0.4
                                    onClicked: NetworkBuffers.apply()
                                    Keys.onReturnPressed: clicked()
                                    Keys.onEnterPressed: clicked()
                                }
                                LavButton {
                                    primary: false
                                    text: qsTr("Copier la commande")
                                    implicitHeight: 48
                                    onClicked: NetworkBuffers.copyCommand()
                                    Keys.onReturnPressed: clicked()
                                    Keys.onEnterPressed: clicked()
                                }
                            }
                        }

                        SettingRow {
                            label: qsTr("Calibrer PyroWave")
                            appWide: true
                            expert: true
                            available: SystemProperties.hasPyroWave && settingsPage.pyroWave
                            focusable: false
                            description: qsTr("Mesure le réseau depuis le PC choisi, puis le débit que ce PC décode et affiche sans à-coups. Environ deux minutes.")
                            Row {
                                readonly property bool hgroup: true
                                spacing: 12
                                LavDropdown {
                                    width: 300
                                    anchors.verticalCenter: parent.verticalCenter
                                    enabled: !PyroWaveCalibrator.running
                                    options: settingsPage.calibrationHostOptions
                                    currentIndex: {
                                        for (var i = 0; i < options.length; i++) {
                                            if (options[i].value === settingsPage.calibrationHostUuid) return i
                                        }
                                        return -1
                                    }
                                    onChosen: (index) => settingsPage.calibrationHostUuid = options[index].value
                                }
                                LavButton {
                                    text: qsTr("Calibrer")
                                    implicitHeight: 48
                                    enabled: !PyroWaveCalibrator.running && settingsPage.calibrationHostUuid !== ""
                                    opacity: enabled ? 1.0 : 0.4
                                    onClicked: {
                                        calibrationDialog.testFps = StreamingPreferences.fps
                                        calibrationDialog.open()
                                        // Each test frame is drawn at this screen's size, as a stream would be
                                        PyroWaveCalibrator.start(ComputerManager, settingsPage.calibrationHostUuid,
                                                                 calibrationDialog.testFps,
                                                                 Math.round(Screen.width * Screen.devicePixelRatio),
                                                                 Math.round(Screen.height * Screen.devicePixelRatio))
                                    }
                                    Keys.onReturnPressed: clicked()
                                    Keys.onEnterPressed: clicked()
                                }
                            }
                        }

                        SettingToggle {
                            objectName: "set:enableYUV444"
                            label: qsTr("Couleurs complètes (4:4:4)")
                            expert: true
                            description: qsTr("Texte plus net sur le bureau ; déconseillé pour les jeux rapides.")
                            checked: StreamingPreferences.enableYUV444
                            onToggled: (v) => {
                                StreamingPreferences.enableYUV444 = v
                                settingsPage.applyDefaultBitrateIfAuto()
                                settingsPage.persist()
                            }
                        }

                        SettingChoice {
                            objectName: "set:videoDecoderSelection"
                            label: qsTr("Décodage vidéo")
                            expert: true
                            description: qsTr("Le décodage logiciel ne sert qu’en cas de souci de pilote.")
                            options: [
                                {text: qsTr("Auto"), value: StreamingPreferences.VDS_AUTO},
                                {text: qsTr("Logiciel"), value: StreamingPreferences.VDS_FORCE_SOFTWARE},
                                {text: qsTr("Matériel"), value: StreamingPreferences.VDS_FORCE_HARDWARE}
                            ]
                            value: StreamingPreferences.videoDecoderSelection
                            onChosen: (v) => { StreamingPreferences.videoDecoderSelection = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:showPerformanceOverlay"
                            label: qsTr("Statistiques à l'écran pendant le stream")
                            description: qsTr("FPS, latence et décodage en surimpression. Ctrl+Alt+Maj+S les affiche ou les masque en jeu.")
                            checked: StreamingPreferences.showPerformanceOverlay
                            onToggled: (v) => { StreamingPreferences.showPerformanceOverlay = v; settingsPage.persist() }
                        }
                    }

                    // ================================= Audio ================================
                    Column {
                        id: pageAudio
                        // rows saved in the active profile (« ● »), but those marked appWide
                        readonly property bool profileRows: true
                        visible: settingsPage.currentPage === "audio"
                        width: parent.width
                        spacing: 12

                        SettingChoice {
                            objectName: "set:audioConfig"
                            label: qsTr("Canaux audio")
                            description: qsTr("À choisir selon ton casque ou tes enceintes.")
                            options: [
                                {text: qsTr("Stéréo"), value: StreamingPreferences.AC_STEREO},
                                {text: "5.1", value: StreamingPreferences.AC_51_SURROUND},
                                {text: "7.1", value: StreamingPreferences.AC_71_SURROUND}
                            ]
                            value: StreamingPreferences.audioConfig
                            onChosen: (v) => { StreamingPreferences.audioConfig = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:playAudioOnHost"
                            label: qsTr("Couper le son du PC distant")
                            description: qsTr("Le son est coupé sur le PC hôte pendant la session.")
                            checked: !StreamingPreferences.playAudioOnHost
                            onToggled: (v) => { StreamingPreferences.playAudioOnHost = !v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:muteOnFocusLoss"
                            label: qsTr("Couper le son quand la fenêtre n'est pas au premier plan")
                            expert: true
                            available: SystemProperties.hasDesktopEnvironment
                            description: qsTr("Silence dès que tu passes sur une autre fenêtre.")
                            checked: StreamingPreferences.muteOnFocusLoss
                            onToggled: (v) => { StreamingPreferences.muteOnFocusLoss = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:micPassthroughEnabled"
                            label: qsTr("Envoyer mon micro au PC hôte")
                            appWide: true
                            description: qsTr("Pour le chat vocal des jeux ou Discord.")
                            checked: ArtemisSettings.micPassthroughEnabled
                            onToggled: (v) => { ArtemisSettings.micPassthroughEnabled = v; settingsPage.persist() }
                        }

                        SettingChoice {
                            objectName: "set:micDeviceName"
                            label: qsTr("Micro utilisé")
                            appWide: true
                            expert: true
                            enabled: ArtemisSettings.micPassthroughEnabled
                            description: qsTr("Un micro branché après coup apparaît dès que tu rouvres la liste.")
                            dropdown: true
                            onListOpening: settingsPage.refreshMicDevices()
                            options: settingsPage.micOptions
                            value: settingsPage.micDevices.indexOf(ArtemisSettings.micDeviceName) < 0 ? "" : ArtemisSettings.micDeviceName
                            onChosen: (v) => { ArtemisSettings.micDeviceName = v; settingsPage.persist() }
                        }

                        SettingSlider {
                            objectName: "set:maxPendingAudioMs"
                            label: qsTr("Tampon audio")
                            appWide: true
                            expert: true
                            description: qsTr("Plus bas, moins de latence ; plus haut, moins de craquements sur un réseau instable. Effet à la prochaine connexion.")
                            descriptionLines: 3
                            from: 10
                            to: 100
                            stepSize: 5
                            value: ArtemisSettings.maxPendingAudioMs
                            valueText: qsTr("%1 ms").arg(ArtemisSettings.maxPendingAudioMs)
                            onMoved: (v) => { ArtemisSettings.maxPendingAudioMs = v; settingsPage.persist() }
                        }
                    }

                    // ================================ Entrées ===============================
                    Column {
                        id: pageEntrees
                        // rows saved in the active profile (« ● »), but those marked appWide
                        readonly property bool profileRows: true
                        visible: settingsPage.currentPage === "entrees"
                        width: parent.width
                        spacing: 12

                        // Same row as the Android client (« Écran tactile », Multitouch / Trackpad, same
                        // texts): the two touch modes the user keeps on both clients.
                        SettingChoice {
                            objectName: "set:absoluteTouchMode"
                            label: qsTr("Écran tactile")
                            // Only with a touchscreen, or when the trackpad is on (a setting that acts
                            // never vanishes from under the user)
                            available: SystemProperties.hasTouchScreen || !StreamingPreferences.absoluteTouchMode
                            description: qsTr("Multitouch : les doigts touchent directement l'écran du PC. Trackpad : le doigt déplace le curseur, appui long pour faire glisser, appui à deux doigts pour le clic droit, deux doigts pour défiler.")
                            descriptionLines: 3
                            options: [
                                {text: qsTr("Multitouch"), value: true},
                                {text: qsTr("Trackpad"), value: false}
                            ]
                            value: StreamingPreferences.absoluteTouchMode
                            onChosen: (v) => { StreamingPreferences.absoluteTouchMode = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:absoluteMouseMode"
                            label: qsTr("Souris pour le bureau à distance")
                            description: qsTr("Le curseur se comporte comme sur ton propre bureau, mais ne fonctionne plus avec les jeux.")
                            checked: StreamingPreferences.absoluteMouseMode
                            onToggled: (v) => { StreamingPreferences.absoluteMouseMode = v; settingsPage.persist() }
                        }

                        // Android's row (checkbox_remember_mouse_mode), same texts: the menu's
                        // « Passer en trackpad / multitouch » then writes « Écran tactile »
                        SettingToggle {
                            objectName: "set:rememberTouchMode"
                            label: qsTr("Retenir le mode tactile choisi en jeu")
                            appWide: true
                            expert: true
                            available: SystemProperties.hasTouchScreen || StreamingPreferences.rememberTouchMode
                            description: qsTr("Sinon, un mode choisi pendant le stream ne vaut que pour ce stream.")
                            checked: StreamingPreferences.rememberTouchMode
                            onToggled: (v) => { StreamingPreferences.rememberTouchMode = v; settingsPage.persist() }
                        }

                        SettingChoice {
                            objectName: "set:captureSysKeysMode"
                            label: qsTr("Envoyer les raccourcis système au PC distant")
                            available: SystemProperties.hasDesktopEnvironment
                            description: qsTr("Alt+Tab, touche Windows… partent au PC distant (pas Ctrl+Alt+Suppr).")
                            options: [
                                {text: qsTr("En plein écran"), value: StreamingPreferences.CSK_FULLSCREEN},
                                {text: qsTr("Toujours"), value: StreamingPreferences.CSK_ALWAYS},
                                {text: qsTr("Jamais"), value: StreamingPreferences.CSK_OFF}
                            ]
                            value: StreamingPreferences.captureSysKeysMode
                            onChosen: (v) => { StreamingPreferences.captureSysKeysMode = v; settingsPage.persist() }
                        }

                    }

                    // =============================== Manettes ===============================
                    Column {
                        id: pageManettes
                        // rows saved in the active profile (« ● »), but those marked appWide
                        readonly property bool profileRows: true
                        visible: settingsPage.currentPage === "manettes"
                        width: parent.width
                        spacing: 12

                        SettingAction {
                            label: qsTr("Assigner les boutons d’une manette")
                            appWide: true
                            description: qsTr("Pour une manette que LavArtemis ne reconnaît pas, ou un bouton mal placé : on clique le contrôle sur le schéma, puis on l'actionne.")
                            buttonText: qsTr("Ouvrir")
                            onTriggered: {
                                settingsPage.saveNow()
                                stackView.push(Qt.createComponent("../GamepadMapper.qml"), {})
                            }
                        }

                        SettingToggle {
                            objectName: "set:swapFaceButtons"
                            label: qsTr("Inverser A/B et X/Y")
                            expert: true
                            description: qsTr("Disposition des boutons façon Nintendo.")
                            checked: StreamingPreferences.swapFaceButtons
                            onToggled: (v) => { StreamingPreferences.swapFaceButtons = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:multiController"
                            label: qsTr("Toujours brancher une manette sur le PC distant")
                            expert: true
                            description: qsTr("Pour les jeux qui ne voient pas une manette branchée après leur lancement.")
                            checked: !StreamingPreferences.multiController
                            onToggled: (v) => { StreamingPreferences.multiController = !v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:backgroundGamepad"
                            label: qsTr("Manette active en arrière-plan")
                            expert: true
                            available: SystemProperties.hasDesktopEnvironment
                            description: qsTr("La manette reste prise en compte quand LavArtemis n'est pas au premier plan.")
                            checked: StreamingPreferences.backgroundGamepad
                            onToggled: (v) => { StreamingPreferences.backgroundGamepad = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:genericGamepadFallbackEnabled"
                            label: qsTr("Deviner la disposition des manettes inconnues")
                            appWide: true
                            expert: true
                            description: qsTr("Une manette non reconnue reçoit une disposition devinée au lieu d'être ignorée.")
                            checked: ArtemisSettings.genericGamepadFallbackEnabled
                            onToggled: (v) => { ArtemisSettings.genericGamepadFallbackEnabled = v; settingsPage.persist() }
                        }
                    }

                    // =============================== Apparence ==============================
                    Column {
                        id: pageApparence
                        visible: settingsPage.currentPage === "apparence"
                        width: parent.width
                        spacing: 12

                        SettingRow {
                            label: qsTr("Thème")
                            description: qsTr("Change l’allure de toute l’appli.")
                            focusable: false
                            Row {
                                readonly property bool hgroup: true
                                spacing: 14
                                Repeater {
                                    model: ["braise", "affiche", "ludique"]
                                    delegate: AbstractButton {
                                        id: themeCard
                                        required property string modelData
                                        readonly property var pal: Theme.palettes[modelData][Theme.dark ? "dark" : "light"]
                                        readonly property bool selected: Theme.id === modelData
                                        width: 196
                                        height: 118
                                        hoverEnabled: true
                                        focusPolicy: Qt.StrongFocus
                                        onClicked: Theme.setTheme(modelData)
                                        Keys.onReturnPressed: clicked()
                                        Keys.onEnterPressed: clicked()
                                        background: Rectangle {
                                            radius: themeCard.modelData === "affiche" ? 0 : (themeCard.modelData === "ludique" ? 24 : 16)
                                            color: themeCard.pal.bg
                                            border.width: themeCard.selected || (themeCard.activeFocus && Theme.showFocus) ? 4 : 1
                                            border.color: themeCard.activeFocus && Theme.showFocus ? Theme.fg
                                                        : (themeCard.selected ? Theme.accent : Theme.line)
                                            Row {
                                                anchors.left: parent.left
                                                anchors.bottom: parent.bottom
                                                anchors.margins: 14
                                                spacing: 8
                                                Repeater {
                                                    model: [themeCard.pal.accent, themeCard.pal.surface2, themeCard.pal.fg]
                                                    delegate: Rectangle {
                                                        required property var modelData
                                                        width: 20; height: 20
                                                        radius: themeCard.modelData === "affiche" ? 0 : 10
                                                        color: modelData
                                                        border.width: 1
                                                        border.color: themeCard.pal.line
                                                    }
                                                }
                                            }
                                        }
                                        contentItem: Item {
                                            Text {
                                                x: 14; y: 12
                                                text: themeCard.modelData === "affiche" ? Theme.themeNames[themeCard.modelData].toUpperCase()
                                                                                        : Theme.themeNames[themeCard.modelData]
                                                color: themeCard.pal.fg
                                                font.pixelSize: 24
                                                font.weight: Font.Black
                                                font.family: themeCard.modelData === "affiche" ? "Archivo"
                                                           : (themeCard.modelData === "ludique" ? "Bricolage Grotesque" : "Space Grotesk")
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        SettingChoice {
                            objectName: "set:themeMode"
                            label: qsTr("Clair ou sombre")
                            description: qsTr("« Système » suit le mode clair ou sombre de l'appareil, en direct.")
                            options: [
                                {text: Theme.modeNames[0], value: 0},
                                {text: Theme.modeNames[1], value: 1},
                                {text: Theme.modeNames[2], value: 2}
                            ]
                            value: Theme.mode
                            onChosen: (v) => { ArtemisSettings.uiThemeMode = v; ArtemisSettings.save() }
                        }

                        SettingChoice {
                            objectName: "set:language"
                            label: qsTr("Langue")
                            description: settingsPage.languageNote !== "" ? settingsPage.languageNote
                                         : qsTr("Prend effet au redémarrage.")
                            dropdown: true
                            options: settingsPage.languageOptions
                            value: StreamingPreferences.language
                            onChosen: (v) => {
                                // Retranslating is expensive, so only when the language actually changed
                                if (StreamingPreferences.language === v) return
                                StreamingPreferences.language = v
                                settingsPage.persist()
                                if (!StreamingPreferences.retranslate()) {
                                    settingsPage.languageNote = qsTr("Redémarre LavArtemis pour appliquer la langue.")
                                } else if (typeof window !== "undefined") {
                                    // The legacy pages under us stop working after retranslate()
                                    window.clearOnBack = true
                                }
                            }
                        }

                        SettingChoice {
                            objectName: "set:uiDisplayMode"
                            label: qsTr("Fenêtre de l'appli au démarrage")
                            available: SystemProperties.hasDesktopEnvironment
                            description: qsTr("Comment LavArtemis s'ouvre, hors stream.")
                            options: [
                                {text: qsTr("Fenêtre"), value: StreamingPreferences.UI_WINDOWED},
                                {text: qsTr("Agrandie"), value: StreamingPreferences.UI_MAXIMIZED},
                                {text: qsTr("Plein écran"), value: StreamingPreferences.UI_FULLSCREEN}
                            ]
                            value: StreamingPreferences.uiDisplayMode
                            onChosen: (v) => { StreamingPreferences.uiDisplayMode = v; settingsPage.persist() }
                        }
                    }

                    // ================================== PC ==================================
                    Column {
                        id: pagePc
                        // rows saved in the active profile (« ● »), but those marked appWide
                        readonly property bool profileRows: true
                        visible: settingsPage.currentPage === "pc"
                        width: parent.width
                        spacing: 12

                        SettingToggle {
                            objectName: "set:quitAppAfter"
                            label: qsTr("Quitter l'appli sur le PC distant à la fin")
                            description: qsTr("Ferme le jeu quand tu arrêtes le stream : la progression non sauvegardée est perdue.")
                            checked: StreamingPreferences.quitAppAfter
                            onToggled: (v) => { StreamingPreferences.quitAppAfter = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:clipboardSync"
                            label: qsTr("Synchroniser le presse-papiers")
                            appWide: true
                            expert: true
                            description: qsTr("Envoie ton presse-papiers au PC distant au début du stream et à chaque copie faite ici, et récupère le sien quand tu te déconnectes. Le menu en jeu le fait aussi à la demande.")
                            descriptionLines: 3
                            checked: ClipboardManager.isEnabled
                            onToggled: (v) => { ClipboardManager.isEnabled = v; settingsPage.persist() }
                        }

                        SettingSlider {
                            objectName: "set:clipboardMaxSizeMB"
                            label: qsTr("Taille maximale")
                            appWide: true
                            expert: true
                            enabled: ClipboardManager.isEnabled
                            description: qsTr("Au-delà, le contenu copié n'est pas synchronisé.")
                            from: 1
                            to: 100
                            stepSize: 1
                            keyStep: 5
                            value: ClipboardManager.maxContentSizeMB
                            valueText: qsTr("%1 Mo").arg(ClipboardManager.maxContentSizeMB)
                            onMoved: (v) => { ClipboardManager.maxContentSizeMB = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:clipboardNotifications"
                            label: qsTr("Notifier chaque synchronisation")
                            appWide: true
                            expert: true
                            enabled: ClipboardManager.isEnabled
                            description: qsTr("Une notification à chaque contenu synchronisé.")
                            checked: ClipboardManager.showNotifications
                            onToggled: (v) => { ClipboardManager.showNotifications = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:enableMdns"
                            label: qsTr("Trouver les PC sur le réseau local")
                            appWide: true
                            expert: true
                            description: qsTr("Détecte tout seul les PC du réseau (recommandé).")
                            checked: StreamingPreferences.enableMdns
                            onToggled: (v) => {
                                StreamingPreferences.enableMdns = v
                                settingsPage.persist()
                                // Restart polling so the mDNS change takes effect
                                if (typeof window !== "undefined" && window.pollingActive) {
                                    ComputerManager.stopPollingAsync()
                                    ComputerManager.startPolling()
                                }
                            }
                        }

                        SettingToggle {
                            objectName: "set:detectNetworkBlocking"
                            label: qsTr("Détecter les connexions bloquées")
                            appWide: true
                            expert: true
                            description: qsTr("Prévient quand un pare-feu ou le réseau bloque le stream (recommandé).")
                            checked: StreamingPreferences.detectNetworkBlocking
                            onToggled: (v) => { StreamingPreferences.detectNetworkBlocking = v; settingsPage.persist() }
                        }
                    }

                    // ============================ Raccourcis clavier ============================

                    // The stream's key combos (streaming/input/input.cpp, m_SpecialKeyCombos): all start with

                    // Ctrl+Alt+Shift. Read-only reference.

                    Column {

                        id: pageRaccourcis

                        visible: settingsPage.currentPage === "raccourcis"

                        width: parent.width

                        spacing: 12


                        Repeater {

                            model: [

                                {key: "\\", label: qsTr("Menu du stream"), desc: qsTr("Ouvre ou ferme le menu pendant le stream.")},

                                {key: "Q", label: qsTr("Déconnecter"), desc: qsTr("Comme « Déconnecter » dans le menu en jeu : arrête le stream et revient à l'accueil.")},

                                {key: "E", label: qsTr("Quitter la session"), desc: qsTr("Comme « Quitter la session » dans le menu en jeu : ferme aussi l'appli sur le PC distant.")},

                                {key: "Z", label: qsTr("Libérer la souris et le clavier"), desc: qsTr("Rend le pointeur à ton PC.")},

                                {key: "X", label: qsTr("Plein écran / fenêtre"), desc: qsTr("Bascule l'affichage du stream.")},

                                {key: "S", label: qsTr("Statistiques"), desc: qsTr("Affiche ou masque les statistiques du stream.")},

                                {key: "M", label: qsTr("Mode de souris"), desc: qsTr("Bascule entre souris de bureau et souris de jeu.")},

                                {key: "C", label: qsTr("Masquer le curseur local"), desc: qsTr("Affiche ou masque le curseur de ton PC.")},

                                {key: "L", label: qsTr("Verrouiller le pointeur"), desc: qsTr("Garde le pointeur dans la fenêtre du stream.")},

                                {key: "K", label: qsTr("Capturer le clavier"), desc: qsTr("Envoie ou non les raccourcis système au PC distant.")},

                                {key: "D", label: qsTr("Réduire la fenêtre"), desc: qsTr("Réduit le stream.")},

                                {key: "V", label: qsTr("Coller du texte"), desc: qsTr("Tape le texte de ton presse-papiers sur le PC distant.")},

                                {key: "U", label: qsTr("Couper / rétablir le micro"), desc: qsTr("Quand le micro est envoyé au PC distant.")},

                                {key: "+  −  0", label: qsTr("Zoom"), desc: qsTr("Zoomer, dézoomer, revenir à 100 %.")},

                                {key: "← → ↑ ↓", label: qsTr("Déplacer la vue zoomée"), desc: qsTr("Quand le stream est zoomé.")}

                            ]

                            delegate: SettingRow {

                                required property var modelData

                                label: modelData.label

                                description: modelData.desc

                                Row {

                                    spacing: 6

                                    Repeater {

                                        model: ["Ctrl", "Alt", "Maj", modelData.key]

                                        delegate: Rectangle {

                                            required property string modelData

                                            height: 34

                                            width: Math.max(34, capText.implicitWidth + 18)

                                            radius: Theme.id === "affiche" ? 0 : 8

                                            color: Theme.surface2

                                            border.width: Math.max(1, Theme.border)

                                            border.color: Theme.borderColor

                                            Text {

                                                id: capText

                                                anchors.centerIn: parent

                                                text: modelData

                                                color: Theme.fg

                                                font.family: Theme.textFont

                                                font.pixelSize: 15

                                                font.weight: Font.Bold

                                            }

                                        }

                                    }

                                }

                            }

                        }

                    }


                    // ================================ Avancé ================================
                    Column {
                        id: pageAvance
                        visible: settingsPage.currentPage === "avance"
                        width: parent.width
                        spacing: 12

                        SettingToggle {
                            objectName: "set:autoUpdateEnabled"
                            label: qsTr("Chercher les mises à jour au démarrage")
                            description: qsTr("Vérifie s'il existe une nouvelle version de LavArtemis.")
                            checked: ArtemisSettings.autoUpdateEnabled
                            onToggled: (v) => { ArtemisSettings.autoUpdateEnabled = v; settingsPage.persist() }
                        }

                        SettingAction {
                            // Once an update is known (here or by the startup check), the same row
                            // installs it: the v2 interface has no toolbar update button.
                            readonly property bool updateKnown: !settingsPage.updateChecking && window.updateVersion !== ""
                            label: updateKnown ? qsTr("Installer la mise à jour")
                                               : qsTr("Chercher une mise à jour maintenant")
                            description: settingsPage.updateChecking ? qsTr("Vérification…")
                                         : (updateKnown ? qsTr("LavArtemis %1 est disponible.").arg(window.updateVersion)
                                         : (settingsPage.updateStatus !== "" ? settingsPage.updateStatus
                                                                             : qsTr("Version installée : %1.").arg(SystemProperties.versionString)))
                            buttonText: updateKnown ? qsTr("Installer") : qsTr("Vérifier")
                            primary: updateKnown
                            busy: settingsPage.updateChecking
                            onTriggered: {
                                if (updateKnown) {
                                    window.openUpdateDialog()
                                    return
                                }
                                settingsPage.updateChecking = true
                                settingsPage.updateStatus = ""
                                // checkNow(), not start(): start() honours the startup preference above
                                AutoUpdateChecker.checkNow()
                            }
                        }

                        SettingToggle {
                            objectName: "set:connectionWarnings"
                            label: qsTr("Avertir quand la connexion se dégrade")
                            expert: true
                            description: qsTr("Un message à l'écran quand le réseau faiblit pendant le stream.")
                            checked: StreamingPreferences.connectionWarnings
                            onToggled: (v) => { StreamingPreferences.connectionWarnings = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:configurationWarnings"
                            label: qsTr("Avertissements de configuration")
                            expert: true
                            // Session::emitLaunchWarning() obeys it, emitLavArtemisWarning() does not
                            description: qsTr("Signale au lancement les réglages que ce PC ou le PC distant ne peut pas appliquer. Micro, multi-écran, VRR, PyroWave et manettes devinées sont toujours signalés.")
                            descriptionLines: 3
                            checked: StreamingPreferences.configurationWarnings
                            onToggled: (v) => { StreamingPreferences.configurationWarnings = v; settingsPage.persist() }
                        }

                        SettingToggle {
                            objectName: "set:perfCsvLoggingEnabled"
                            label: qsTr("Enregistrer les performances (CSV)")
                            expert: true
                            description: qsTr("Une ligne par seconde (FPS, décodage, latence), un fichier par stream dans le dossier de l'appli. À couper après la mesure.")
                            checked: ArtemisSettings.perfCsvLoggingEnabled
                            onToggled: (v) => { ArtemisSettings.perfCsvLoggingEnabled = v; settingsPage.persist() }
                        }

                        SettingAction {
                            label: qsTr("Ouvrir le dossier des mesures")
                            expert: true
                            available: ArtemisSettings.perfCsvLoggingEnabled
                            description: qsTr("Les fichiers « stream-perf » des streams enregistrés.")
                            buttonText: qsTr("Ouvrir")
                            onTriggered: ArtemisSettings.openPerfCsvFolder()
                        }

                        SettingToggle {
                            objectName: "set:traceVrrFrames"
                            label: qsTr("Trace détaillée du VRR")
                            expert: true
                            available: StreamingPreferences.enableVrr || StreamingPreferences.traceVrrFrames
                            description: qsTr("Traces dans le dossier « vrr-diagnostics » de l'appli, dès la prochaine connexion. Prend de la place : à couper après.")
                            checked: StreamingPreferences.traceVrrFrames
                            onToggled: (v) => { StreamingPreferences.traceVrrFrames = v; settingsPage.persist() }
                        }

                        SettingAction {
                            label: qsTr("Exporter la dernière trace VRR (ZIP)")
                            expert: true
                            available: StreamingPreferences.enableVrr || StreamingPreferences.traceVrrFrames
                            description: VrrDiagnostics.status !== "" ? VrrDiagnostics.status
                                         : qsTr("Un ZIP de la dernière session tracée, à joindre à un rapport.")
                            buttonText: VrrDiagnostics.exporting ? qsTr("Export…") : qsTr("Exporter")
                            busy: VrrDiagnostics.exporting
                            onTriggered: VrrDiagnostics.exportLatest()
                        }

                        SettingAction {
                            label: qsTr("Ouvrir le dossier des traces")
                            expert: true
                            available: StreamingPreferences.enableVrr || StreamingPreferences.traceVrrFrames
                            description: qsTr("Le dossier « vrr-diagnostics » de l'appli.")
                            buttonText: qsTr("Ouvrir")
                            onTriggered: VrrDiagnostics.openFolder()
                        }
                    }

                    // Prototype V8: the expert rows of this page, while expert mode is off
                    AbstractButton {
                        id: hiddenButton
                        readonly property int hidden: settingsPage.hiddenCount(settingsPage.currentPage, ArtemisSettings.uiExpertMode)
                        visible: hidden > 0
                        width: parent.width
                        height: 57
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        onClicked: { ArtemisSettings.uiExpertMode = true; ArtemisSettings.save() }
                        Keys.onReturnPressed: clicked()
                        Keys.onEnterPressed: clicked()
                        background: DashedRect {
                            radius: Theme.rs
                            borderColor: Theme.muted
                            FocusRing { active: hiddenButton.activeFocus && Theme.showFocus; radius: Theme.rs; k: 1.25 }
                        }
                        contentItem: Text {
                            text: (hiddenButton.hidden > 1 ? qsTr("%1 réglages avancés masqués").arg(hiddenButton.hidden)
                                                           : qsTr("%1 réglage avancé masqué").arg(hiddenButton.hidden))
                                  + " · " + qsTr("activer le mode expert")
                            color: Theme.muted
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.family: Theme.textFont
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }
        }
    }

    // ---- dialogs (window overlay, outside the scaled stage) ------------------------------------
    LavDialog {
        id: profileNameDialog
        // Empty means creating rather than renaming
        property string renamingId: ""
        title: renamingId !== "" ? qsTr("Renommer le profil") : qsTr("Nouveau profil")
        text: renamingId !== "" ? "" : qsTr("Il part des réglages en cours.")
        acceptText: renamingId !== "" ? qsTr("Renommer") : qsTr("Créer")
        acceptEnabled: profileNameField.text.trim().length > 0
        initialFocus: profileNameField.input
        fields: [
            LavTextField {
                id: profileNameField
                width: profileNameDialog.bodyWidth
                placeholderText: qsTr("Nom du profil")
                maximumLength: 60
                onAccepted: profileNameDialog.accept()
            }
        ]
        onAboutToShow: profileNameField.text = renamingId !== "" ? ProfileManager.activeProfileName : ""
        onAccepted: {
            var name = profileNameField.text.trim()
            if (renamingId !== "") {
                ProfileManager.renameProfile(renamingId, name)
            } else {
                // createProfile() takes a full snapshot of what is in effect (unsaved edits first);
                // switching to it and saving writes the members as they are now
                settingsPage.saveNow()
                var id = ProfileManager.createProfile(name)
                ProfileManager.setActiveProfile(id)
                StreamingPreferences.save()
            }
        }
    }

    LavDialog {
        id: deleteProfileDialog
        title: qsTr("Supprimer le profil")
        text: qsTr("Supprimer le profil « %1 » ? Les réglages qu'il contient seront perdus.").arg(ProfileManager.activeProfileName)
        acceptText: qsTr("Supprimer")
        onAccepted: ProfileManager.deleteProfile(ProfileManager.activeProfileId)
    }

    LavDialog {
        id: customResolutionDialog
        title: qsTr("Résolution personnalisée")
        text: qsTr("Une résolution que ce PC ou le PC distant ne gère pas peut faire échouer le stream.")
        acceptText: qsTr("Appliquer")
        initialFocus: widthField.input
        function valid(field) {
            return field.text.length === 0 || field.acceptableInput
        }
        acceptEnabled: valid(widthField) && valid(heightField)
        fields: [
            Row {
                spacing: 14
                LavTextField {
                    id: widthField
                    width: 200
                    maximumLength: 5
                    placeholderText: String(StreamingPreferences.width)
                    validator: IntValidator { bottom: 256; top: 8192 }
                    onAccepted: customResolutionDialog.accept()
                }
                Text {
                    text: "×"
                    color: Theme.fg
                    font.pixelSize: 26
                    font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
                LavTextField {
                    id: heightField
                    width: 200
                    maximumLength: 5
                    placeholderText: String(StreamingPreferences.height)
                    validator: IntValidator { bottom: 256; top: 8192 }
                    onAccepted: customResolutionDialog.accept()
                }
            }
        ]
        onAboutToShow: { widthField.text = ""; heightField.text = "" }
        onAccepted: {
            var w = parseInt(widthField.text.length > 0 ? widthField.text : widthField.placeholderText)
            var h = parseInt(heightField.text.length > 0 ? heightField.text : heightField.placeholderText)
            if (w > 0 && h > 0) settingsPage.setResolution(w, h)
        }
    }

    LavDialog {
        id: customFpsDialog
        title: qsTr("Images par seconde personnalisées")
        text: qsTr("De 10 à 500, avec deux décimales au plus (59,94 par exemple).")
        acceptText: qsTr("Appliquer")
        initialFocus: fpsField.input
        function rate() {
            var r = parseFloat(fpsField.text.replace(",", "."))
            return (isNaN(r) || r < 10 || r > 500) ? 0 : r
        }
        acceptEnabled: fpsField.acceptableInput && rate() > 0
        fields: [
            LavTextField {
                id: fpsField
                width: 220
                maximumLength: 6
                placeholderText: "144"
                validator: RegularExpressionValidator { regularExpression: /^[0-9]{1,3}([.,][0-9]{0,2})?$/ }
                onAccepted: customFpsDialog.accept()
            }
        ]
        onAboutToShow: fpsField.text = settingsPage.fractionalRate
                                       ? StreamingPreferences.customRefreshRate.toFixed(2).replace(".", ",") : ""
        onAccepted: {
            var r = rate()
            if (r <= 0) return
            if (Math.round(r) === r) {
                settingsPage.setFps(r)
            } else {
                // A fractional rate (59.94): the stream asks for it exactly
                var changed = StreamingPreferences.fps !== Math.round(r)
                StreamingPreferences.customRefreshRate = r
                StreamingPreferences.enableFractionalRefreshRate = true
                StreamingPreferences.fps = Math.round(r)
                if (changed) settingsPage.applyDefaultBitrateIfAuto()
                settingsPage.persist()
            }
        }
    }

    // « Saisir un débit », Android's texts (SettingsActivity.askBitrate)
    LavDialog {
        id: customBitrateDialog
        title: qsTr("Saisir un débit")
        text: qsTr("En Mbit/s, avec des décimales si besoin.")
        acceptText: qsTr("Appliquer")
        initialFocus: bitrateField.input
        function kbps() {
            var m = parseFloat(bitrateField.text.replace(",", "."))
            return isNaN(m) ? 0 : Math.round(m * 1000)
        }
        acceptEnabled: kbps() > 0
        fields: [
            LavTextField {
                id: bitrateField
                width: 220
                maximumLength: 7
                placeholderText: "40"
                validator: RegularExpressionValidator { regularExpression: /^[0-9]{0,4}([.,][0-9]{0,1})?$/ }
                onAccepted: customBitrateDialog.accept()
            }
        ]
        onAboutToShow: bitrateField.text = settingsPage.mbps(StreamingPreferences.bitrateKbps)
        onAccepted: {
            var v = kbps()
            if (v <= 0) return
            StreamingPreferences.bitrateKbps = v
            StreamingPreferences.autoAdjustBitrate = false
            settingsPage.persist()
        }
    }

    PyroWaveCalibration {
        id: calibrationDialog
        onApplied: (option) => settingsPage.applyCalibration(option)
    }
}
