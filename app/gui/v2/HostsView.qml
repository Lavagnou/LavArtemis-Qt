import QtQuick
import QtQuick.Controls

import ".."
import ComputerModel 1.0
import ComputerManager 1.0

// "Mes PC": every host LavArtemis knows, with its state. A paired online host opens on the home;
// others offer what makes sense (pair, wake). Adding by address and the advanced actions
// (OTP pairing, rename, remove, benchmark) stay in the previous screen for now.
FocusScope {
    id: hostsPage
    objectName: qsTr("Mes PC")
    focus: true

    readonly property bool isV2Page: true
    property var home: null
    property ComputerModel computerModel: createModel()

    function createModel() {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', hostsPage, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        return model
    }

    function pairingComplete(error) {
        pairDialog.close()
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.open()
        }
    }


    HoverHandler {
        onPointChanged: if (Theme.input !== "mouse" && point.device.type === PointerDevice.Mouse) Theme.input = "mouse"
    }
    Keys.onPressed: (event) => {
        if (Theme.input !== "keys") Theme.input = "keys"
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) { stackView.pop(); event.accepted = true }
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Stage {
        anchors.fill: parent

        Flickable {
            anchors.fill: parent
            contentHeight: column.implicitHeight + 60
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: column
                x: 50; y: 6 // Close under the title bar, which is empty and in the theme's colours (see main.qml themedFrame)
                width: parent.width - 100
                spacing: 22

                PageHeader {
                    title: qsTr("Mes PC")
                    onBack: stackView.pop()
                }

                Repeater {
                    model: hostsPage.computerModel
                    delegate: AbstractButton {
                        id: hostRow
                        required property int index
                        required property string name
                        required property bool online
                        required property bool paired
                        required property bool busy
                        required property bool wakeable
                        required property bool statusUnknown
                        required property string uuid

                        width: column.width
                        height: 96
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        focus: index === 0

                        readonly property string status: statusUnknown ? qsTr("Recherche…")
                            : (!online ? qsTr("Hors ligne") : (!paired ? qsTr("Non appairé")
                            : (busy ? qsTr("Session en cours") : qsTr("En ligne"))))

                        function activate() {
                            if (online && paired) {
                                if (hostsPage.home) hostsPage.home.showHost(uuid)
                                stackView.pop()
                            } else if (online && !paired) {
                                var pin = hostsPage.computerModel.generatePinString()
                                hostsPage.computerModel.pairComputer(index, pin)
                                pairDialog.pin = pin
                                pairDialog.open()
                            } else if (!online && wakeable) {
                                hostsPage.computerModel.wakeComputer(index)
                                toast.show(qsTr("Réveil de %1 envoyé").arg(name))
                            }
                        }
                        onClicked: activate()
                        Keys.onReturnPressed: activate()

                        transform: Translate { y: hostRow.hovered && Theme.input === "mouse" ? -3 : 0 }

                        background: Rectangle {
                            radius: Theme.r
                            color: Theme.surface
                            border.width: hostRow.activeFocus && Theme.showFocus ? 3 : Theme.border
                            border.color: hostRow.activeFocus && Theme.showFocus ? Theme.accent : Theme.borderColor
                        }
                        contentItem: Item {
                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 28
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 22
                                Rectangle {
                                    width: 14; height: 14; radius: 7
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: hostRow.online ? (hostRow.paired ? Theme.accent : Theme.muted) : Theme.off
                                }
                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    Text {
                                        text: hostRow.name
                                        color: Theme.fg
                                        font.family: Theme.displayFont
                                        font.pixelSize: 26
                                        font.weight: Theme.displayWeight
                                    }
                                    Text {
                                        text: hostRow.status
                                        color: Theme.muted
                                        font.family: Theme.textFont
                                        font.pixelSize: 16
                                    }
                                }
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 28
                                anchors.verticalCenter: parent.verticalCenter
                                text: hostRow.online && hostRow.paired ? qsTr("Ouvrir")
                                    : (hostRow.online ? qsTr("Appairer") : (hostRow.wakeable ? qsTr("Réveiller") : ""))
                                color: Theme.accent
                                font.family: Theme.textFont
                                font.pixelSize: 18
                                font.weight: Font.Bold
                            }
                        }
                    }
                }

                Row {
                    spacing: 16
                    LavButton {
                        text: qsTr("Ajouter ou gérer un PC")
                        iconName: "plus"
                        primary: false
                        onClicked: {
                            // The previous screen keeps adding by address, OTP pairing, renaming,
                            // removing and the benchmark until they move here.
                            var component = Qt.createComponent("../PcView.qml")
                            stackView.push(component.createObject(stackView, {}))
                        }
                    }
                }
            }
        }
    }

    NavigableMessageDialog {
        id: pairDialog
        closePolicy: Popup.CloseOnEscape
        property string pin: "0000"
        text: qsTr("Saisis le code %1 sur le PC hôte (interface web de LavApollo). Cette fenêtre se fermera une fois l'appairage terminé.").arg(pin)
        standardButtons: Dialog.Cancel
    }

    ErrorMessageDialog { id: errorDialog }

    Rectangle {
        id: toast
        function show(message) { toastText.text = message; opacity = 1; toastTimer.restart() }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        width: toastText.implicitWidth + 40; height: 48
        radius: Theme.pill
        color: Theme.fg
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Text { id: toastText; anchors.centerIn: parent; color: Theme.bg; font.pixelSize: 16 }
        Timer { id: toastTimer; interval: 2500; onTriggered: toast.opacity = 0 }
    }
}
