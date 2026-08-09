import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

import SdlGamepadMapper 1.0
import SdlGamepadKeyNavigation 1.0

Item {
    id: mapperPage
    objectName: qsTr("Gamepad Mapping")

    property var devices: []

    function refreshDevices() {
        devices = SdlGamepadMapper.enumerateDevices()
    }

    // Gamepad UI navigation reads the very same buttons we're trying to
    // record. Left enabled, every press would move the focus instead of being
    // captured.
    StackView.onActivated: {
        SdlGamepadKeyNavigation.disable()
        refreshDevices()
    }

    StackView.onDeactivating: {
        SdlGamepadMapper.cancel()
        SdlGamepadKeyNavigation.enable()
    }

    Component.onDestruction: {
        SdlGamepadMapper.cancel()
    }

    // ---- Device picker ----------------------------------------------------

    ColumnLayout {
        id: devicePicker
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        visible: !SdlGamepadMapper.mapping

        Label {
            text: qsTr("Gamepad Mapper")
            font.pointSize: 20
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Pick a device to teach LavArtemis its button layout. Controllers already recognized don't need this unless something is misplaced.")
            font.pointSize: 10
            color: "#aaaaaa"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("Refresh")
            onClicked: refreshDevices()
        }

        ListView {
            id: deviceList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: mapperPage.devices
            spacing: 4

            delegate: ItemDelegate {
                width: deviceList.width
                height: 64

                onClicked: SdlGamepadMapper.startMapping(modelData.index)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    spacing: 2

                    Label {
                        text: modelData.name
                        font.pointSize: 12
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        // The status is the whole reason this screen exists,
                        // so it says which of the three states applies.
                        text: {
                            if (modelData.guessed) {
                                return qsTr("Layout guessed — may be wrong")
                            }
                            else if (modelData.recognized) {
                                return qsTr("Recognized")
                            }
                            else {
                                return qsTr("Not recognized — unusable until mapped")
                            }
                        }
                        color: modelData.guessed ? "#e0b040" : (modelData.recognized ? "#60c060" : "#e06060")
                        font.pointSize: 9
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("%1 axes, %2 buttons, %3 hats")
                                .arg(modelData.numAxes).arg(modelData.numButtons).arg(modelData.numHats)
                        color: "#888888"
                        font.pointSize: 8
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Label {
            visible: mapperPage.devices.length === 0
            text: qsTr("No game controllers or joysticks found.")
            color: "#aaaaaa"
            font.pointSize: 11
            Layout.fillWidth: true
        }
    }

    // ---- Capture wizard ---------------------------------------------------

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16
        visible: SdlGamepadMapper.mapping

        Label {
            text: SdlGamepadMapper.deviceName
            font.pointSize: 16
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: SdlGamepadMapper.stepCount
            value: SdlGamepadMapper.currentStep
        }

        Label {
            text: qsTr("Step %1 of %2")
                    .arg(Math.min(SdlGamepadMapper.currentStep + 1, SdlGamepadMapper.stepCount))
                    .arg(SdlGamepadMapper.stepCount)
            color: "#aaaaaa"
            font.pointSize: 9
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }

        Label {
            text: SdlGamepadMapper.currentPrompt
            font.pointSize: 22
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("If this control doesn't exist on your device, skip it.")
            color: "#aaaaaa"
            font.pointSize: 10
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }

        Label {
            id: mappingPreview
            text: SdlGamepadMapper.previewMapping()
            color: "#777777"
            font.pointSize: 8
            font.family: "monospace"
            wrapMode: Text.Wrap
            Layout.fillWidth: true

            Connections {
                target: SdlGamepadMapper
                function onCurrentStepChanged() {
                    mappingPreview.text = SdlGamepadMapper.previewMapping()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: qsTr("Skip")
                onClicked: SdlGamepadMapper.skipCurrentStep()
            }

            Button {
                text: qsTr("Start over")
                onClicked: SdlGamepadMapper.restart()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Cancel")
                onClicked: SdlGamepadMapper.cancel()
            }

            Button {
                text: qsTr("Save")
                // Saving a mapping with nothing bound would replace a working
                // guess with an empty entry.
                enabled: SdlGamepadMapper.bindingCount > 0
                highlighted: true
                onClicked: {
                    if (SdlGamepadMapper.saveMapping()) {
                        refreshDevices()
                    }
                }
            }
        }
    }

    Connections {
        target: SdlGamepadMapper
        function onMappingComplete() {
            savePrompt.open()
        }
    }

    NavigableMessageDialog {
        id: savePrompt
        standardButtons: Dialog.Save | Dialog.Cancel
        text: qsTr("All controls have been visited. Save this mapping?")
        onAccepted: {
            SdlGamepadMapper.saveMapping()
            refreshDevices()
        }
        onRejected: SdlGamepadMapper.cancel()
    }
}
