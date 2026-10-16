import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Material 2.2
import QtQuick.Dialogs
import QtQuick.Layouts 1.2

import BenchmarkRunner 1.0
import ComputerManager 1.0

// Benchmark of one host's streaming settings (backend/benchmark, LavApollo docs/benchmark.md).
// Pushed from the host's context menu. Each run is a StreamSegue pushed on top of this page,
// which stays underneath for the whole benchmark.
Item {
    id: benchmarkPage
    objectName: qsTr("Benchmark: %1").arg(hostName)

    property string hostUuid
    property string hostName
    property string mode: "quick"
    property string goal: "balanced"
    property var plan: ({})
    property var historyList: []
    // "setup" until a report is shown, either a fresh one or one from the history
    property bool showingReport: false

    readonly property color betterColor: "#4caf50"
    readonly property color worseColor: "#f44336"

    function refreshPlan() {
        plan = BenchmarkRunner.describe(ComputerManager, hostUuid, mode, goal)
    }

    function refreshHistory() {
        historyList = BenchmarkRunner.history(hostUuid)
    }

    function signed(value, digits) {
        return (value > 0 ? "+" : "") + value.toFixed(digits)
    }

    function deltaColor(value, higherIsBetter) {
        if (Math.abs(value) < 0.05) {
            return Material.foreground
        }
        return (value > 0) === higherIsBetter ? betterColor : worseColor
    }

    Component.onCompleted: {
        refreshPlan()
        refreshHistory()
    }

    // Leaving the page ends the benchmark after the current run
    Component.onDestruction: BenchmarkRunner.cancel()

    StackView.onActivated: {
        refreshPlan()
        refreshHistory()
    }

    Connections {
        target: BenchmarkRunner

        function onSessionReady(session, label) {
            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {"appName": label, "session": session, "benchmarkMode": true})
            stackView.push(segue)
        }

        function onFinished() {
            showingReport = true
            refreshHistory()
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 20
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: content
            width: parent.width
            spacing: 14

            // ---- Setup ------------------------------------------------------

            ColumnLayout {
                visible: !BenchmarkRunner.running && !showingReport
                Layout.fillWidth: true
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: qsTr("Streams an animated scene from %1 several times in a row, each time with another codec, bitrate or frame pacing, and scores the latency and smoothness of each against your current settings. Your settings are not changed unless you apply a result.").arg(hostName)
                }

                Label {
                    text: qsTr("Test")
                    font.bold: true
                }

                ButtonGroup { id: modeGroup }
                Repeater {
                    model: [
                        { "id": "quick", "name": qsTr("Quick"), "desc": qsTr("Your settings, the other codecs, two other pacing modes.") },
                        { "id": "full", "name": qsTr("Full"), "desc": qsTr("Every codec at three bitrates, then every pacing mode.") },
                        { "id": "adaptive", "name": qsTr("Adaptive"), "desc": qsTr("Finds the best codec, then its best bitrate, then the best pacing, towards your goal.") }
                    ]
                    delegate: RadioButton {
                        Layout.fillWidth: true
                        ButtonGroup.group: modeGroup
                        checked: benchmarkPage.mode === modelData.id
                        text: modelData.name + " - " + modelData.desc
                        onToggled: {
                            benchmarkPage.mode = modelData.id
                            refreshPlan()
                        }
                    }
                }

                RowLayout {
                    spacing: 10
                    Label {
                        text: qsTr("Goal")
                        font.bold: true
                    }
                    ComboBox {
                        id: goalBox
                        textRole: "name"
                        model: [
                            { "id": "balanced", "name": qsTr("Balanced") },
                            { "id": "latency", "name": qsTr("Lowest latency") },
                            { "id": "smoothness", "name": qsTr("Smoothest") }
                        ]
                        onActivated: {
                            benchmarkPage.goal = model[currentIndex].id
                            refreshPlan()
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: plan.available === true
                    text: qsTr("Your settings: %1. About %n run(s), %2 minutes. Don't use the PC while it runs: anything else on screen or on the network changes the results.", "", plan.runs || 0)
                              .arg(plan.baseline || "").arg(plan.minutes || 0)
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: plan.otherAppRunning === true
                    color: worseColor
                    text: qsTr("An app is running on the host. It will be quit.")
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: plan.available === false
                    color: worseColor
                    text: plan.reason || ""
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: BenchmarkRunner.message !== ""
                    text: BenchmarkRunner.message
                }

                RowLayout {
                    spacing: 10
                    Button {
                        text: qsTr("Start")
                        highlighted: true
                        enabled: plan.available === true
                        onClicked: BenchmarkRunner.start(ComputerManager, hostUuid, benchmarkPage.mode, benchmarkPage.goal)
                    }
                    Button {
                        text: qsTr("Open a report file...")
                        onClicked: importDialog.open()
                    }
                }

                Label {
                    text: qsTr("Earlier benchmarks")
                    font.bold: true
                    visible: historyList.length > 0
                }

                Repeater {
                    model: historyList
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: modelData.createdAt + " - " + modelData.mode + ", " + modelData.goal
                                  + (modelData.platform === "android" ? " (Android)" : "")
                                  + (modelData.best ? " - " + qsTr("best: %1").arg(modelData.best) : "")
                                  + (modelData.cancelled ? " - " + qsTr("stopped") : "")
                                  + (modelData.unstable ? " - " + qsTr("unstable") : "")
                        }
                        Button {
                            text: qsTr("Open")
                            onClicked: {
                                if (BenchmarkRunner.openReport(modelData.id)) {
                                    showingReport = true
                                }
                            }
                        }
                        Button {
                            text: qsTr("Delete")
                            onClicked: {
                                BenchmarkRunner.deleteReport(modelData.id)
                                refreshHistory()
                            }
                        }
                    }
                }
            }

            // ---- Running ----------------------------------------------------

            ColumnLayout {
                visible: BenchmarkRunner.running
                Layout.fillWidth: true
                spacing: 12

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: BenchmarkRunner.running
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: BenchmarkRunner.message
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Stop")
                    onClicked: BenchmarkRunner.cancel()
                }
            }

            // ---- Report -----------------------------------------------------

            ColumnLayout {
                id: reportView
                property var report: BenchmarkRunner.report
                visible: (showingReport || BenchmarkRunner.running) && report.runs !== undefined && report.runs.length > 0
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.bold: true
                    text: qsTr("%1, %2 - %3, goal: %4 - %5").arg(reportView.report.hostName || "").arg(reportView.report.createdAt || "")
                              .arg(reportView.report.mode || "").arg(reportView.report.goal || "").arg(reportView.report.stream || "")
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: reportView.report.unstable === true
                    color: worseColor
                    text: qsTr("Your settings scored %1 points differently at the start and at the end: conditions changed during the benchmark, so small differences don't mean much. Run it again.").arg((reportView.report.baselineDrift || 0).toFixed(1))
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: reportView.report.applicable === false
                    text: qsTr("This report was made on Android: its settings can't be applied here.")
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.8
                    text: qsTr("Latency and smoothness are scored out of 100, ranked by your goal. Arrows compare each result with your settings; ± is how much the score moved within the run.")
                }

                Repeater {
                    model: reportView.report.ranked || []
                    delegate: Pane {
                        Layout.fillWidth: true
                        padding: 10
                        Material.elevation: modelData.index === reportView.report.bestIndex ? 4 : 1

                        ColumnLayout {
                            width: parent.width
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                font.bold: true
                                text: (modelData.index === reportView.report.bestIndex ? "★ " : "")
                                      + (modelData.roleLabel ? modelData.roleLabel + ": " : "") + modelData.label
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                visible: modelData.status !== "ok"
                                color: modelData.status === "fallback" ? Material.accent : worseColor
                                text: (modelData.status === "skipped" ? qsTr("Skipped") :
                                       modelData.status === "fallback" ? qsTr("Not tested as asked") : qsTr("Failed"))
                                      + (modelData.note ? ": " + modelData.note : "")
                            }

                            RowLayout {
                                visible: modelData.status === "ok"
                                spacing: 18
                                Label {
                                    text: qsTr("Latency %1 ±%2").arg(modelData.latency.toFixed(1)).arg(modelData.spreadLatency.toFixed(1))
                                }
                                Label {
                                    visible: modelData.comparable
                                    color: deltaColor(modelData.deltaLatency, true)
                                    text: (modelData.deltaLatency >= 0 ? "▲ " : "▼ ") + signed(modelData.deltaLatency, 1)
                                }
                                Label {
                                    text: qsTr("Smoothness %1 ±%2").arg(modelData.smoothness.toFixed(1)).arg(modelData.spreadSmoothness.toFixed(1))
                                }
                                Label {
                                    visible: modelData.comparable
                                    color: deltaColor(modelData.deltaSmoothness, true)
                                    text: (modelData.deltaSmoothness >= 0 ? "▲ " : "▼ ") + signed(modelData.deltaSmoothness, 1)
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                visible: modelData.status === "ok"
                                opacity: 0.85
                                text: qsTr("%1 ms estimated (host %2, network %3, receive %4, decode %5, queue %6, render %7) - %8% of frames shown, %9% lost - frame times p50 %10 / p99 %11 ms, %12% hitches")
                                          .arg(modelData.metrics.totalMs.toFixed(1))
                                          .arg(modelData.metrics.hostMs.toFixed(1)).arg(modelData.metrics.networkMs.toFixed(1))
                                          .arg(modelData.metrics.receiveMs.toFixed(1)).arg(modelData.metrics.decodeMs.toFixed(1))
                                          .arg(modelData.metrics.queueMs.toFixed(1)).arg(modelData.metrics.renderMs.toFixed(1))
                                          .arg(modelData.metrics.deliveredPct.toFixed(1)).arg(modelData.metrics.networkLossPct.toFixed(2))
                                          .arg(modelData.metrics.presentP50Ms.toFixed(2)).arg(modelData.metrics.presentP99Ms.toFixed(2))
                                          .arg(modelData.metrics.stutterPct.toFixed(2))
                            }

                            Label {
                                visible: modelData.comparable
                                opacity: 0.85
                                color: deltaColor(modelData.deltaTotalMs, false)
                                text: qsTr("%1 ms compared with your settings").arg(signed(modelData.deltaTotalMs, 1))
                            }

                            RowLayout {
                                visible: !BenchmarkRunner.running && modelData.status === "ok"
                                         && reportView.report.applicable === true && modelData.role === "candidate"
                                spacing: 10
                                Button {
                                    text: qsTr("Apply to my settings")
                                    onClicked: BenchmarkRunner.applyRun(modelData.index)
                                }
                                Button {
                                    text: qsTr("Save as a profile")
                                    onClicked: BenchmarkRunner.applyRunAsProfile(modelData.index, qsTr("Benchmark: %1").arg(modelData.label))
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: showingReport && !BenchmarkRunner.running && BenchmarkRunner.message !== ""
                    text: BenchmarkRunner.message
                }

                Button {
                    visible: showingReport && !BenchmarkRunner.running
                    text: qsTr("Back")
                    onClicked: {
                        showingReport = false
                        refreshPlan()
                        refreshHistory()
                    }
                }
            }
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Open a benchmark report")
        nameFilters: [qsTr("Benchmark report (*.json)")]
        onAccepted: {
            if (BenchmarkRunner.importReport(selectedFile)) {
                showingReport = true
                refreshHistory()
            }
        }
    }
}
