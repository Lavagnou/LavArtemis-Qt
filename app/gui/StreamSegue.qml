import QtQuick 2.0
import QtQuick.Controls 2.2
import QtQuick.Window 2.2

import BenchmarkRunner 1.0
import SdlGamepadKeyNavigation 1.0
import Session 1.0
import SystemProperties 1.0
import "v2"

Item {
    property Session session
    property string appName
    // The prototype's segue (v2/SegueScreen): the picture, « Lancement sur ATLAS-PC… », the tip
    property string hostName: session ? session.hostName : ""
    property url coverSource: ""
    property url bgSource: ""
    // Another device streams from this host (« Rejoindre »): « Connexion à la session de … »
    property bool joining: false
    // Said over the stream once it is on screen (« Connecté à ATLAS-PC », « Session reprise »…)
    property string startToast: ""
    // The home page (HomeView): the stream ends there, as the prototype (ingame → home), with
    // « Session terminée » or « Déconnecté : la session continue sur le PC »
    property var home: null
    property bool quitting: false
    property string stageText : isRestart ? qsTr("Host display changed: reconnecting…") :
                                isResume ? qsTr("Resuming %1...").arg(appName) :
                                           qsTr("Starting %1...").arg(appName)
    property bool isResume : false
    // The reconnection after a host display change (Session::createRestartSession): says so the
    // whole time (Android: host_display_changed), and replays no launch warning
    property bool isRestart : false
    property bool quitAfter : false
    // A benchmark run (BenchmarkView): no warning toasts to wait for, and errors go to
    // the benchmark's report rather than a dialog in the middle of the sequence
    property bool benchmarkMode : false

    function stageStarting(stage)
    {
        // Update the spinner text (the stage's name comes translated: Session, stageLabel)
        if (!isRestart) {
            stageText = qsTr("Starting: %1").arg(stage)
        }
    }

    function stageFailed(stage, errorCode, failingPorts)
    {
        // Display the error dialog after Session::exec() returns (Android: conn_error_stage)
        streamSegueErrorDialog.text = qsTr("Start failed: %1 (error %2)").arg(stage).arg(errorCode)

        if (failingPorts) {
            streamSegueErrorDialog.text += "\n\n" + qsTr("Check your firewall and port forwarding rules for port(s): %1").arg(failingPorts)
        }
    }

    function connectionStarted()
    {
        // Hide the UI contents so the user doesn't
        // see them briefly when we pop off the StackView
        segueScreen.visible = false

        // Hide the window now that streaming has begun
        window.visible = false
    }

    function displayLaunchError(text)
    {
        // Display the error dialog after Session::exec() returns
        streamSegueErrorDialog.text = text
        console.error(text)
    }

    function quitStarting()
    {
        quitting = true
        // Avoid the push transition animation
        var component = Qt.createComponent("QuitSegue.qml")
        stackView.replace(stackView.currentItem, component.createObject(stackView, {"appName": appName, "hostName": hostName,
                                                                                    "coverSource": coverSource, "bgSource": bgSource}),
                          StackView.Immediate)

        // Show the Qt window again to show quit segue
        window.visible = true
    }

    function sessionFinished(portTestResult)
    {
        // The host display changed mode mid-session: reconnect to the same session at the new
        // mode, without going back to the app list (Session::hostDisplayWatchTick)
        if (session && session.isRestartRequested() && !streamSegueErrorDialog.text) {
            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {
                                                   "appName": appName,
                                                   "session": session.createRestartSession(),
                                                   "isResume": true,
                                                   "isRestart": true,
                                                   "hostName": hostName,
                                                   "coverSource": coverSource,
                                                   "bgSource": bgSource,
                                                   "home": home,
                                                   "quitAfter": quitAfter,
                                                   "benchmarkMode": benchmarkMode
                                               })
            stackView.replace(stackView.currentItem, segue, StackView.Immediate)
            // The stream window is gone until the new session is up: say what happens meanwhile
            window.visible = true
            return
        }

        if (portTestResult !== 0 && portTestResult !== -1 && streamSegueErrorDialog.text) {
            streamSegueErrorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking LavArtemis. Streaming over the Internet may not work while connected to this network.")
        }

        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()

        // Pop the StreamSegue off the stack if this is a GUI-based app launch: back to the home page
        // when it launched the stream (from the library or the details too, as the prototype)
        if (!quitAfter) {
            if (home && stackView.find(function(item) { return item === home }) !== null) {
                stackView.pop(home)
                if (!streamSegueErrorDialog.text && !benchmarkMode) {
                    home.say(quitting ? qsTr("Session terminée") : qsTr("Déconnecté : la session continue sur le PC"))
                }
            } else {
                stackView.pop()
            }
        }

        if (quitAfter && !streamSegueErrorDialog.text) {
            // If this was a CLI launch without errors, exit now
            Qt.quit()
        }
        else {
            // Show the Qt window again after streaming
            window.visible = true

            // Display any launch errors. We do this after
            // the Qt UI is visible again to prevent losing
            // focus on the dialog which would impact gamepad
            // users.
            if (streamSegueErrorDialog.text && !benchmarkMode) {
                streamSegueErrorDialog.quitAfter = quitAfter
                streamSegueErrorDialog.open()
            }
        }
    }

    function sessionReadyForDeletion()
    {
        // Garbage collect the Session object since it's pretty heavyweight
        // and keeps other libraries (like SDL_TTF) around until it is deleted.
        session = null
        gc()
    }

    StackView.onDeactivating: {
        // Re-enable GUI gamepad usage now
        SdlGamepadKeyNavigation.enable()
    }

    StackView.onActivated: {
        // Hook up our signals
        session.stageStarting.connect(stageStarting)
        session.stageFailed.connect(stageFailed)
        session.connectionStarted.connect(connectionStarted)
        session.displayLaunchError.connect(displayLaunchError)
        session.quitStarting.connect(quitStarting)
        session.sessionFinished.connect(sessionFinished)
        session.readyForDeletion.connect(sessionReadyForDeletion)

        // Ensure the SystemProperties async thread is finished,
        // since it may currently be using the SDL video subsystem
        SystemProperties.waitForAsyncLoad()

        // Kick off the stream (the segue screen shows from the start: no spinner to delay)
        streamLoader.active = true
    }

    Timer {
        id: startSessionTimer
        onTriggered: {
            // Garbage collect QML stuff before we start streaming,
            // since we'll probably be streaming for a while and we
            // won't be able to GC during the stream.
            gc()

            // Run the streaming session to completion
            session.start()
        }
    }

    Loader {
        id: streamLoader
        active: false
        asynchronous: true

        onLoaded: {
            // Set the hint text. We do this here rather than
            // in the hintText control itself to synchronize
            // with Session.exec() which requires no concurrent
            // gamepad usage.
            segueScreen.tip = Theme.input === "pad" || SdlGamepadKeyNavigation.getConnectedGamepads() > 0
                    ? qsTr("Astuce : Start + Select + LB + RB te déconnecte pendant le stream")
                    : qsTr("Astuce : Ctrl + Alt + Maj + Q te déconnecte pendant le stream")
            if (!benchmarkMode && !isRestart) {
                session.startToast = startToast
            }

            // Stop GUI gamepad usage now
            SdlGamepadKeyNavigation.disable()

            // Initialize the session and probe for host/client capabilities
            if (!session.initialize(window)) {
                if (benchmarkMode) {
                    BenchmarkRunner.abortRun(streamSegueErrorDialog.text)
                }
                sessionFinished(0);
                sessionReadyForDeletion();
                return;
            }

            // Don't wait unless we have toasts to display
            startSessionTimer.interval = 0

            // Display the toasts together in a vertical centered arrangement
            var yOffset = 0
            // Not again for the reconnection: they were shown at the launch, and nothing would
            // show them now but a hidden window, 3.5 s each
            for (var i = 0; i < session.launchWarnings.length && !benchmarkMode && !isRestart; i++) {
                var text = session.launchWarnings[i]
                console.warn(text)

                // Show the tooltip for 3 seconds
                var toast = Qt.createQmlObject('import QtQuick.Controls 2.2; ToolTip {}', parent, '')
                toast.timeout = 3000
                toast.text = text
                toast.y += yOffset
                toast.visible = true

                // Offset the next toast below the previous one
                yOffset = toast.y + toast.padding + toast.height

                // Allow an extra 500 ms for the tooltip's fade-out animation to finish
                startSessionTimer.interval = toast.timeout + 500;
            }

            // Start the timer to wait for toasts (or start the session immediately)
            startSessionTimer.start()
        }

        sourceComponent: Item {}
    }

    SegueScreen {
        id: segueScreen
        anchors.fill: parent
        title: appName
        cover: coverSource
        background: bgSource
        line: isRestart ? stageText
            : isResume && joining ? qsTr("Connexion à la session de %1…").arg(hostName)
            : isResume ? qsTr("Reprise sur %1…").arg(hostName)
            : qsTr("Lancement sur %1…").arg(hostName)
    }
}
