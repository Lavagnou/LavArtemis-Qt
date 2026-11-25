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
        updateDialog.open()
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
    height: 600
    // Below this the redesigned pages can only shrink and break (Stage.minLayoutWidth is 1000):
    // the window stops there, unless the screen itself is smaller.
    minimumWidth: Math.min(1000, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(600, Screen.desktopAvailableHeight)

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

    // Update dialog: offers an in-app download on Windows, or opens the
    // release page on platforms without self-install (macOS, Linux AppImage).
    NavigableMessageDialog {
        id: updateDialog
        // « Ignorer cette version », as Android: the startup check no longer offers it
        // (AutoUpdateChecker::skipVersion), « Vérifier » in Réglages still does
        standardButtons: Dialog.Ok | Dialog.Cancel | Dialog.Discard
        onAboutToShow: {
            var skip = standardButton(Dialog.Discard)
            if (skip) skip.text = qsTr("Ignorer cette version")
        }
        onDiscarded: {
            AutoUpdateChecker.skipVersion(window.updateVersion)
            window.updateVersion = ""
            close()
        }

        property bool canDownload: AutoUpdateChecker.canSelfInstall() && window.updateDownloadUrl !== ""

        text: qsTr("LavArtemis %1 is available. You are running version %2.")
                  .arg(window.updateVersion).arg(SystemProperties.versionString) +
              "\n\n" +
              (canDownload ? qsTr("Press OK to download and install it.")
                           : qsTr("Press OK to open the download page in your browser."))

        onAccepted: {
            if (canDownload) {
                updateDownloadDialog.open()
            } else if (SystemProperties.hasBrowser) {
                Qt.openUrlExternally(window.updateBrowserUrl)
            }
        }
    }

    // The download gets its own dialog because pressing a standard button closes
    // the dialog it belongs to. Driving the progress bar from updateDialog's own
    // OK handler drew it on a dialog that had already gone away, which is why
    // accepting an update looked like it did nothing at all.
    NavigableDialog {
        id: updateDownloadDialog

        property bool downloading: false
        property string downloadedPath: ""
        property string errorText: ""

        // Nothing to accept until the download resolves one way or the other.
        standardButtons: downloading ? Dialog.Cancel : (Dialog.Ok | Dialog.Cancel)

        // A click outside the dialog must not silently abandon a running download.
        closePolicy: Popup.NoAutoClose

        onOpened: {
            downloading = true
            downloadedPath = ""
            errorText = ""
            updateProgressBar.value = 0
            updateProgressBar.indeterminate = true
            AutoUpdateChecker.downloadUpdate(window.updateDownloadUrl)
        }

        onAccepted: {
            if (downloadedPath !== "") {
                AutoUpdateChecker.installAndRestart(downloadedPath)
            } else if (errorText !== "" && SystemProperties.hasBrowser) {
                // The in-app path failed, so hand them the release page instead.
                Qt.openUrlExternally(window.updateBrowserUrl)
            }
        }

        onRejected: {
            if (downloading) {
                AutoUpdateChecker.cancelDownload()
            }
        }

        Component.onCompleted: {
            AutoUpdateChecker.downloadProgress.connect(function(received, total) {
                if (total > 0) {
                    updateProgressBar.indeterminate = false
                    updateProgressBar.value = received / total
                }
            })
            AutoUpdateChecker.downloadReady.connect(function(path) {
                updateDownloadDialog.downloading = false
                updateDownloadDialog.downloadedPath = path
            })
            AutoUpdateChecker.downloadFailed.connect(function(error) {
                updateDownloadDialog.downloading = false
                updateDownloadDialog.errorText = error
            })
        }

        ColumnLayout {
            Label {
                text: {
                    if (updateDownloadDialog.downloading) {
                        return qsTr("Downloading LavArtemis %1…").arg(window.updateVersion)
                    }
                    else if (updateDownloadDialog.downloadedPath !== "") {
                        return qsTr("LavArtemis %1 is ready to install. Press OK to install it and restart.")
                                  .arg(window.updateVersion)
                    }
                    else if (SystemProperties.hasBrowser) {
                        return qsTr("Download failed: %1").arg(updateDownloadDialog.errorText) +
                               "\n\n" + qsTr("Press OK to open the download page in your browser.")
                    }
                    else {
                        return qsTr("Download failed: %1").arg(updateDownloadDialog.errorText)
                    }
                }
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.maximumWidth: 400
            }

            ProgressBar {
                id: updateProgressBar
                Layout.fillWidth: true
                visible: updateDownloadDialog.downloading
                from: 0
                to: 1
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
