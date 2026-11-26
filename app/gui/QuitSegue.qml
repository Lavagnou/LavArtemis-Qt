import QtQuick 2.0
import QtQuick.Controls 2.2

import ComputerManager 1.0
import Session 1.0
import "v2"

Item {
    property string appName
    property var quitRunningAppFn
    property Session nextSession : null
    property string nextAppName : ""
    // The prototype's segue (v2/SegueScreen, planche V8-25): « Fermeture sur ATLAS-PC… »
    property string hostName: ""
    property url coverSource: ""
    property url bgSource: ""
    // The StreamSegue that follows (« Fermer X et lancer », relaunch): its picture, toast, home
    property var nextSegueProps: ({})
    // The home page: « Session terminée » once the app is closed and nothing follows
    property var home: null

    property string stageText : qsTr("Quitting %1...").arg(appName)

    function quitAppCompleted(error)
    {
        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.open()
            console.error(error)
        }

        // If we're supposed to launch another game after this, do so now
        if (error === undefined && nextSession !== null) {
            var component = Qt.createComponent("StreamSegue.qml")
            var props = {"appName": nextAppName, "session": nextSession}
            for (var key in nextSegueProps) props[key] = nextSegueProps[key]
            var segue = component.createObject(stackView, props)
            stackView.replace(segue)
        }
        else {
            // Exit this view
            stackView.pop()
            if (error === undefined && home && stackView.currentItem === home) {
                home.say(qsTr("Session terminée"))
            }
        }
    }

    StackView.onActivated: {
        // Connect the quit completion signal
        ComputerManager.quitAppCompleted.connect(quitAppCompleted)

        // Start the quit operation if requested
        if (quitRunningAppFn) {
            quitRunningAppFn()
        }
    }

    StackView.onDeactivating: {
        // Disconnect the signal
        ComputerManager.quitAppCompleted.disconnect(quitAppCompleted)
    }

    SegueScreen {
        anchors.fill: parent
        title: appName
        cover: coverSource
        background: bgSource
        line: hostName.length > 0 ? qsTr("Fermeture sur %1…").arg(hostName) : stageText
    }

    ErrorMessageDialog {
        id: errorDialog
    }
}
