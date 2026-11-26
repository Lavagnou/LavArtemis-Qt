import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2
import QtQuick.Controls.Material 2.2

import ComputerManager 1.0
import AutoUpdateChecker 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0
import ArtemisSettings 1.0
import "v2"

ApplicationWindow {
    property bool pollingActive: false

    // Set by the settings when the language changes (retranslate()): going back then pops every
    // page but the first, built before the change.
    property bool clearOnBack: false

    // Set when a check (at startup or "check now") finds an update: the home ("Mise à jour") and
    // Réglages › Avancé ("Installer") read it and call openUpdateDialog().
    property string updateVersion: ""
    property string updateBrowserUrl: ""
    property string updateDownloadUrl: ""

    function openUpdateDialog() {
        updateOptions.offer()
    }

    // The native title bar takes the theme's background and text colours (Windows 11) instead of
    // the system accent colour.
    readonly property bool themedFrame: true
    readonly property string frameKey: themedFrame ? ("" + Theme.bg + Theme.fg + Theme.dark) : "system"
    onFrameKeyChanged: applyFrameColors()
    function applyFrameColors() {
        SystemProperties.setWindowFrameColors(window, Theme.bg, Theme.fg, Theme.dark, !themedFrame)
    }

    id: window
    // Test build: the window never takes the focus (shown with SW_SHOWNOACTIVATE, WS_EX_NOACTIVATE)
    flags: lavTestBuild ? (Qt.Window | Qt.WindowDoesNotAcceptFocus) : Qt.Window
    // The theme's background from the very first frame, under every page; the Material controls
    // still used (message dialogs, stream segues, mapper, benchmark) take the theme's colours
    background: Rectangle { color: Theme.bg }
    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.background: Theme.surface
    Material.foreground: Theme.fg
    Material.accent: Theme.accent
    Material.primary: Theme.surface
    // Controls without a font of their own (mapper, benchmark, message dialogs): the theme's
    font.family: Theme.textFont
    font.pixelSize: 16
    width: 1280
    height: 720
    // Below this the redesigned pages can only shrink and break (Stage.minLayoutWidth is 1000):
    // the window stops there, unless the screen itself is smaller.
    minimumWidth: Math.min(1000, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(600, Screen.desktopAvailableHeight)

    // No shader effects on the software scene graph (offscreen captures): see Theme.effects
    Binding { target: Theme; property: "effects"; value: window.GraphicsInfo.api !== GraphicsInfo.Software }

    // This function runs prior to creation of the initial StackView item
    function doEarlyInit() {
        SdlGamepadKeyNavigation.enable()
    }

    function updateAvailable(version, url) {
        window.updateVersion = version
        window.updateBrowserUrl = url
    }

    Component.onCompleted: {
        applyFrameColors()

        AutoUpdateChecker.onUpdateAvailable.connect(updateAvailable)
        AutoUpdateChecker.onUpdateDownloadUrl.connect(function(url) { window.updateDownloadUrl = url })
        AutoUpdateChecker.start()

        // Show the window according to the user's preferences
        if (lavTestBuild) {
            // Test build: plain window, never maximized or fullscreen, shown without activation
            window.show()
        }
        else if (SystemProperties.hasDesktopEnvironment) {
            if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_MAXIMIZED) {
                window.showMaximized()
            }
            else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_FULLSCREEN) {
                window.showFullScreen()
            }
            else {
                window.show()
            }
        } else {
            window.showFullScreen()
        }

        // A malformed art:// link or .art file opened the app: say what was wrong with it
        if (startupError !== "") {
            startupErrorDialog.text = startupError
            startupErrorDialog.open()
        }

        // Display any modal dialogs for configuration warnings
        if (runConfigChecks) {
            if (SystemProperties.isWow64) {
                wow64Dialog.open()
            }

            // Hardware acceleration and unmapped gamepads are checked asynchronously
            SystemProperties.hasHardwareAccelerationChanged.connect(hasHardwareAccelerationChanged)
            SystemProperties.unmappedGamepadsChanged.connect(hasUnmappedGamepadsChanged)
            SystemProperties.startAsyncLoad()
        }
    }

    function hasHardwareAccelerationChanged() {
        if (!SystemProperties.hasHardwareAcceleration && StreamingPreferences.videoDecoderSelection !== StreamingPreferences.VDS_FORCE_SOFTWARE) {
            if (SystemProperties.isRunningXWayland) {
                xWaylandDialog.open()
            }
            else {
                noHwDecoderDialog.open()
            }
        }
    }

    function hasUnmappedGamepadsChanged() {
        if (SystemProperties.unmappedGamepads) {
            unmappedGamepadDialog.unmappedGamepads = SystemProperties.unmappedGamepads
            unmappedGamepadDialog.open()
        }
    }

    // It would be better to use TextMetrics here, but it always lays out
    // the text slightly more compactly than real Text does in ToolTip,
    // causing unexpected line breaks to be inserted
    Text {
        id: tooltipTextLayoutHelper
        visible: false
        font: ToolTip.toolTip.font
        text: ToolTip.toolTip.text
    }

    // This configures the maximum width of the singleton attached QML ToolTip. If left unconstrained,
    // it will never insert a line break and just extend on forever.
    // Note: ToolTip must be attached to an Item, not ApplicationWindow
    Item {
        id: tooltipHelper
        ToolTip.toolTip.contentWidth: Math.min(tooltipTextLayoutHelper.width, 400)
    }

    function goBack() {
        if (clearOnBack) {
            // Pop all items except the first one
            stackView.pop(null)
            clearOnBack = false
        }
        else {
            stackView.pop()
        }
    }

    StackView {
        id: stackView
        anchors.fill: parent
        focus: true

        Component.onCompleted: {
            // Perform our early initialization before constructing
            // the initial view and pushing it to the StackView
            doEarlyInit()
            push(initialView)
        }

        onCurrentItemChanged: {
            // Ensure focus travels to the next view when going back
            if (currentItem) {
                currentItem.forceActiveFocus()
            }
        }

        Keys.onEscapePressed: {
            if (depth > 1) {
                goBack()
            }
            else {
                quitConfirmationDialog.open()
            }
        }

        Keys.onBackPressed: {
            if (depth > 1) {
                goBack()
            }
            else {
                quitConfirmationDialog.open()
            }
        }
    }

    // This timer keeps us polling for 5 minutes of inactivity
    // to allow the user to work with Moonlight on a second display
    // while dealing with configuration issues. This will ensure
    // machines come online even if the input focus isn't on Moonlight.
    Timer {
        id: inactivityTimer
        interval: 5 * 60000
        onTriggered: {
            if (!active && pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
    }

    onVisibleChanged: {
        // When we become invisible while streaming is going on,
        // stop polling immediately.
        if (!visible) {
            inactivityTimer.stop()

            if (pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
        else if (active || lavTestBuild) {
            // When we become visible and active again, start polling.
            // Test build: the window never becomes active (it never takes the focus), so being
            // visible counts as active, as a real user's window would be.
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }

        // Poll for gamepad input only when the window is in focus
        SdlGamepadKeyNavigation.notifyWindowFocus(visible && active)
    }

    onActiveChanged: {
        if (active) {
            // Stop the inactivity timer
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }
        else if (!lavTestBuild) {
            // Start the inactivity timer to stop polling
            // if focus does not return within a few minutes.
            inactivityTimer.restart()
        }

        // Poll for gamepad input only when the window is in focus
        SdlGamepadKeyNavigation.notifyWindowFocus(visible && active)
    }

    ErrorMessageDialog {
        id: startupErrorDialog
    }

    ErrorMessageDialog {
        id: noHwDecoderDialog
        text: qsTr("No functioning hardware accelerated video decoder was detected by LavArtemis. " +
                   "Your streaming performance may be severely degraded in this configuration.")
    }

    ErrorMessageDialog {
        id: xWaylandDialog
        text: qsTr("Hardware acceleration doesn't work on XWayland. Continuing on XWayland may result in poor streaming performance. " +
                   "Try running with QT_QPA_PLATFORM=wayland or switch to X11.")
    }

    NavigableMessageDialog {
        id: wow64Dialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        text: qsTr("This version of LavArtemis isn't optimized for your PC. Please download the '%1' version of LavArtemis for the best streaming performance.").arg(SystemProperties.friendlyNativeArchName)
        onAccepted: {
            Qt.openUrlExternally("https://github.com/Lavagnou/LavArtemis/releases");
        }
    }

    // Sending people to a wiki page made sense when there was nothing in the
    // app to send them to. Now the mapper can fix the device on the spot.
    NavigableMessageDialog {
        id: unmappedGamepadDialog
        property string unmappedGamepads : ""
        standardButtons: Dialog.Ok | Dialog.Cancel
        text: qsTr("These gamepads have no usable mapping and won't work:") + "\n" + unmappedGamepads +
              "\n\n" + qsTr("Open the gamepad mapper to set one up?")
        onAccepted: stackView.push("qrc:/gui/GamepadMapper.qml")
    }

    // Update (prototype V8-13, ctxKind update / updating): the theme's centred dialog. « Télécharger et
    // installer » downloads the installer here (Windows), the bar follows the download, then « Installer et
    // redémarrer ». Without self-install (macOS, Linux AppImage, a release without installer) the main entry
    // opens the release page. « Ignorer cette version », as Android: the startup check no longer offers it
    // (AutoUpdateChecker::skipVersion), « Vérifier » in Réglages still does.
    // The download keeps its own state: a dialog closed while it runs (« Plus tard » is not offered then)
    // cannot abandon it silently; only « Annuler le téléchargement » stops it.
    LavOptions {
        id: updateOptions
        property bool canDownload: AutoUpdateChecker.canSelfInstall() && window.updateDownloadUrl !== ""
        property bool downloading: false
        property string downloadedPath: ""
        property real progress: 0
        readonly property string headText: qsTr("Mise à jour") + " · LavArtemis"

        function offer() {
            if (downloading || downloadedPath !== "") return showProgress()
            showDialog({head: headText, title: qsTr("LavArtemis %1 est disponible").arg(window.updateVersion), info: true,
                        note: canDownload
                              ? qsTr("Tu as la version %1. La nouvelle se télécharge et s’installe, puis LavArtemis redémarre.").arg(SystemProperties.versionString)
                              : qsTr("Tu as la version %1. Elle n’a pas d’installeur pour ce PC : sa page s’ouvre dans ton navigateur.").arg(SystemProperties.versionString),
                        entries: [{l: canDownload ? qsTr("Télécharger et installer") : qsTr("Ouvrir la page de la version"), a: "updGo", main: true},
                                  {l: qsTr("Ignorer cette version"), a: "updSkip"},
                                  {l: qsTr("Plus tard"), a: "cancel"}]})
        }
        function showProgress() {
            var ready = downloadedPath !== ""
            showDialog({head: headText, info: ready, extra: updateProgress,
                        title: ready ? qsTr("LavArtemis %1 est prête à installer").arg(window.updateVersion)
                                     : qsTr("Téléchargement de LavArtemis %1…").arg(window.updateVersion),
                        note: ready ? qsTr("Installe-la : LavArtemis se ferme, puis redémarre.") : "",
                        entries: ready ? [{l: qsTr("Installer et redémarrer"), a: "updInstall", main: true}, {l: qsTr("Plus tard"), a: "cancel"}]
                                       : [{l: qsTr("Annuler le téléchargement"), a: "updStop", main: true}]})
        }
        function failed(error) {
            showDialog({head: headText, title: qsTr("Oups"), info: true,
                        note: qsTr("Le téléchargement a échoué : %1").arg(error),
                        entries: SystemProperties.hasBrowser
                                 ? [{l: qsTr("Ouvrir la page de la version"), a: "updPage", main: true}, {l: qsTr("OK"), a: "cancel"}]
                                 : [{l: qsTr("OK"), a: "cancel", main: true}]})
        }

        onPicked: (a) => {
            if (a === "updGo") {
                if (!canDownload) {
                    if (SystemProperties.hasBrowser) Qt.openUrlExternally(window.updateBrowserUrl)
                    return
                }
                downloading = true
                downloadedPath = ""
                progress = 0
                AutoUpdateChecker.downloadUpdate(window.updateDownloadUrl)
                showProgress()
            } else if (a === "updSkip") {
                AutoUpdateChecker.skipVersion(window.updateVersion)
                window.updateVersion = ""
            } else if (a === "updStop") {
                AutoUpdateChecker.cancelDownload()
                downloading = false
            } else if (a === "updInstall") {
                AutoUpdateChecker.installAndRestart(downloadedPath)
            } else if (a === "updPage") {
                Qt.openUrlExternally(window.updateBrowserUrl)
            }
        }
        onClosed: if (stackView.currentItem) stackView.currentItem.forceActiveFocus()

        Component.onCompleted: {
            AutoUpdateChecker.downloadProgress.connect(function(received, total) {
                if (total > 0) updateOptions.progress = received / total
            })
            AutoUpdateChecker.downloadReady.connect(function(path) {
                updateOptions.downloading = false
                updateOptions.downloadedPath = path
                updateOptions.progress = 1
                updateOptions.showProgress()
            })
            AutoUpdateChecker.downloadFailed.connect(function(error) {
                if (!updateOptions.downloading) return  // cancelled
                updateOptions.downloading = false
                updateOptions.failed(error)
            })
        }
    }
    // The bar of the download (prototype hasProgress: « Téléchargement… » / « Téléchargée » and the %)
    Item {
        id: updateProgress
        visible: false
        readonly property real k: 1.25 * Theme.stageScale
        height: progressLabel.height + 18 * k
        Text {
            id: progressLabel
            text: updateOptions.downloadedPath !== "" ? qsTr("Téléchargée") : qsTr("Téléchargement…")
            color: Theme.fg
            font.family: Theme.textFont
            font.weight: Font.Bold
            font.pixelSize: 14 * updateProgress.k
        }
        Text {
            anchors.right: parent.right
            text: Math.round(updateOptions.progress * 100) + " %"
            color: Theme.fg
            font.family: Theme.textFont
            font.weight: Font.Bold
            font.pixelSize: 14 * updateProgress.k
        }
        Rectangle {
            y: progressLabel.height + 8 * updateProgress.k
            width: parent.width
            height: 10 * updateProgress.k
            radius: Math.min(height / 2, Theme.pill * updateProgress.k)
            clip: true
            color: Theme.off
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, updateOptions.progress))
                height: parent.height
                radius: parent.radius
                color: Theme.on
            }
        }
    }

    // This dialog appears when quitting via keyboard or gamepad button
    NavigableMessageDialog {
        id: quitConfirmationDialog
        standardButtons: Dialog.Yes | Dialog.No
        text: qsTr("Are you sure you want to quit?")
        // For keyboard/gamepad navigation
        onAccepted: Qt.quit()
    }

    // HACK: This belongs in StreamSegue but keeping a dialog around after the parent
    // dies can trigger bugs in Qt 5.12 that cause the app to crash. For now, we will
    // host this dialog in a QML component that is never destroyed.
    //
    // To repro: Start a stream, cut the network connection to trigger the "Connection
    // terminated" dialog, wait until the app grid times out back to the PC grid, then
    // try to dismiss the dialog.
    ErrorMessageDialog {
        id: streamSegueErrorDialog

        property bool quitAfter: false

        onClosed: {
            if (quitAfter) {
                Qt.quit()
            }

            // StreamSegue assumes its dialog will be re-created each time we
            // start streaming, so fake it by wiping out the text each time.
            text = ""
        }
    }
}
