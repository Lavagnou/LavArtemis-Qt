import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

import SdlGamepadMapper 1.0
import SdlGamepadKeyNavigation 1.0

Item {
    id: mapperPage
    objectName: qsTr("Gamepad Mapping")

    focus: true

    property var devices: []
    // True while "map everything" is walking the whole controller, so that
    // finishing the queue can offer to save. A one-off click shouldn't.
    property bool walkthroughRunning: false

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

    // Escape means "stop waiting for a control" first and "leave the page"
    // second. Without consuming it, the StackView's own handler would pop the
    // view out from under a capture.
    Keys.onEscapePressed: function(event) {
        if (SdlGamepadMapper.targetElement !== "") {
            SdlGamepadMapper.cancelSelection()
            walkthroughRunning = false
            event.accepted = true
        }
        else {
            event.accepted = false
        }
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

    // ---- Mapping diagram --------------------------------------------------

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10
        visible: SdlGamepadMapper.mapping

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: SdlGamepadMapper.deviceName
                font.pointSize: 16
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            // Raw device activity, independent of any binding. If this stays
            // empty while the controller is being operated, the problem is the
            // device or the driver, not the mapping.
            Label {
                text: SdlGamepadMapper.rawActivity
                color: "#00cccc"
                font.pointSize: 10
                font.family: "monospace"
                horizontalAlignment: Text.AlignRight
                Layout.minimumWidth: 160
            }
        }

        // The instruction and the capture prompt share one fixed-height strip
        // rather than the prompt overlaying the diagram: the triggers sit at
        // the very top of the controller, and a banner would cover exactly the
        // control being asked for. A fixed height also keeps the diagram from
        // resizing under the user's cursor when a capture starts.
        Rectangle {
            id: promptStrip
            readonly property bool listening: SdlGamepadMapper.targetElement !== ""

            Layout.fillWidth: true
            Layout.preferredHeight: 54
            color: listening ? "#2d2d2d" : "transparent"
            border.color: listening ? "#e0b040" : "transparent"
            border.width: 2
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 14

                Label {
                    Layout.fillWidth: true
                    text: promptStrip.listening
                          ? SdlGamepadMapper.targetPrompt
                          : qsTr("Click a control, then operate it on your controller. Right-click a control to clear it.")
                    color: promptStrip.listening ? "#ffffff" : "#aaaaaa"
                    font.pointSize: promptStrip.listening ? 14 : 10
                    font.bold: promptStrip.listening
                    elide: Text.ElideRight
                }

                Label {
                    visible: SdlGamepadMapper.queueRemaining > 0
                    text: qsTr("%1 to go").arg(SdlGamepadMapper.queueRemaining)
                    color: "#aaaaaa"
                    font.pointSize: 10
                }

                Button {
                    text: qsTr("Skip")
                    flat: true
                    visible: promptStrip.listening
                    onClicked: SdlGamepadMapper.skipTarget()
                }

                Button {
                    text: qsTr("Stop")
                    flat: true
                    visible: promptStrip.listening
                    onClicked: {
                        SdlGamepadMapper.cancelSelection()
                        mapperPage.walkthroughRunning = false
                    }
                }
            }
        }

        ControllerDiagram {
            id: diagram
            Layout.fillWidth: true
            Layout.fillHeight: true

            bindings: SdlGamepadMapper.bindings
            values: SdlGamepadMapper.elementValues
            target: SdlGamepadMapper.targetElement

            onSelectRequested: function(elements) {
                // Picking a control by hand replaces the queue, so a
                // walkthrough that was running is over -- otherwise finishing
                // this one control would pop the "save?" prompt.
                mapperPage.walkthroughRunning = false
                SdlGamepadMapper.selectElements(elements)
            }
            onClearRequested: function(elements) {
                for (var i = 0; i < elements.length; i++) {
                    SdlGamepadMapper.clearElement(elements[i])
                }
            }
        }

        Label {
            id: mappingPreview
            text: SdlGamepadMapper.previewMapping()
            color: "#777777"
            font.pointSize: 8
            font.family: "monospace"
            wrapMode: Text.Wrap
            Layout.fillWidth: true

            // previewMapping() is a function call, so it would be evaluated
            // once and never again without this.
            Connections {
                target: SdlGamepadMapper
                function onBindingsChanged() {
                    mappingPreview.text = SdlGamepadMapper.previewMapping()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: qsTr("Map everything in order")
                onClicked: {
                    mapperPage.walkthroughRunning = true
                    SdlGamepadMapper.mapEverything()
                }
            }

            Button {
                text: qsTr("Clear all")
                enabled: SdlGamepadMapper.bindingCount > 0
                onClicked: SdlGamepadMapper.clearAll()
            }

            Label {
                visible: savedNotice.running
                text: qsTr("Mapping saved")
                color: "#60c060"
                font.pointSize: 10
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Back to device list")
                onClicked: {
                    SdlGamepadMapper.cancel()
                    mapperPage.walkthroughRunning = false
                    refreshDevices()
                }
            }

            Button {
                text: qsTr("Save")
                // Saving a mapping with nothing bound would replace a working
                // guess with an empty entry.
                enabled: SdlGamepadMapper.bindingCount > 0
                highlighted: true
                onClicked: {
                    if (SdlGamepadMapper.saveMapping()) {
                        savedNotice.restart()
                        refreshDevices()
                    }
                }
            }
        }
    }

    Timer {
        id: savedNotice
        interval: 3000
    }

    Connections {
        target: SdlGamepadMapper
        function onQueueFinished() {
            if (mapperPage.walkthroughRunning) {
                mapperPage.walkthroughRunning = false
                savePrompt.open()
            }
        }
    }

    NavigableMessageDialog {
        id: savePrompt
        standardButtons: Dialog.Save | Dialog.Cancel
        text: qsTr("All controls have been visited. Save this mapping?")
        onAccepted: {
            if (SdlGamepadMapper.saveMapping()) {
                savedNotice.restart()
                refreshDevices()
            }
        }
    }
}
