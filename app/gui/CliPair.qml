import QtQuick 2.0
import QtQuick.Controls 2.2

import ComputerManager 1.0

// Pairing from the command line or an art:// pairing link (cli/pair.cpp). The texts are those of
// « Mes PC » and of Android (v2_pair_text, v2_pair_link_text, v2_paired); on success the PC opens
// on the home, as Android opens it after a pairing link (an already paired PC included).
Item {
    id: cliPair
    property string pairedUuid: ""

    function onSearchingComputer(target) {
        stageLabel.text = qsTr("Connexion à %1…").arg(target)
    }

    function onPairing(pcName, pin, fromLink) {
        // A link carries the code (Apollo's OTP): nothing to type anywhere
        stageLabel.text = fromLink
                ? qsTr("Appairage de %1 avec le code du lien… Cette fenêtre se fermera toute seule.").arg(pcName)
                : qsTr("Sur %1, ouvre LavApollo › Appairage et entre ce code : %2. Il n'est demandé qu'une fois ; cette fenêtre se fermera toute seule.")
                      .arg(pcName).arg(pin.split("").join(" "))
    }

    function onFailed(message) {
        stageIndicator.visible = false
        errorDialog.text = message
        errorDialog.open()
    }

    function onSuccess(uuid, pcName) {
        stageIndicator.visible = false
        pairedUuid = uuid
        pairCompleteDialog.text = qsTr("%1 est appairé").arg(pcName)
        pairCompleteDialog.open()
    }

    // The home, on the PC just paired (main.qml's stack: this page is replaced)
    function openPairedPc() {
        var home = stackView.replace(null, "qrc:/gui/v2/HomeView.qml")
        if (home && pairedUuid !== "") home.showHost(pairedUuid)
    }

    // Allow user to back out of pairing
    Keys.onEscapePressed: {
        Qt.quit()
    }
    Keys.onBackPressed: {
        Qt.quit()
    }
    Keys.onCancelPressed: {
        Qt.quit()
    }

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.pairing.connect(onPairing)
            launcher.failed.connect(onFailed)
            launcher.success.connect(onSuccess)
            launcher.execute(ComputerManager)
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 5
        id: stageIndicator

        BusyIndicator {
            id: stageSpinner
            running: visible
        }

        Label {
            id: stageLabel
            // The PIN sentence is long: it wraps inside the window
            width: Math.min(implicitWidth, cliPair.width - stageSpinner.width - 80)
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter

            wrapMode: Text.Wrap
        }
    }

    ErrorMessageDialog {
        id: errorDialog

        onClosed: {
            Qt.quit();
        }
    }

    NavigableMessageDialog {
        id: pairCompleteDialog
        closePolicy: Popup.CloseOnEscape

        standardButtons: Dialog.Ok
        onClosed: openPairedPc()
    }
}
