import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

import BenchmarkRunner 1.0
import ComputerManager 1.0
import "v2"

// « Benchmark des réglages » of one host (backend/benchmark, LavApollo docs/benchmark.md), laid out as the
// prototype (screen "bench", planches V8-43 setup, V8-44 running, V8-45 results): header with the host, the
// setup (Test / Objectif segments, summary, warning, « Lancer le benchmark ») with « Benchmarks précédents » on
// the right, the running step, then the ranked results and their actions. Pushed from the menu of a PC in Mes PC.
// Each run is a StreamSegue pushed on top of this page, which stays underneath for the whole benchmark.
// Prototype px x k (1.25 on the 1600 stage).
FocusScope {
    id: page
    objectName: qsTr("Benchmark : %1").arg(hostName)
    focus: true

    property string hostUuid
    property string hostName
    // « ATLAS-PC · LavApollo 1.2.0 » in the header (HostsView passes the version when the host gives it)
    property string hostSoft: ""
    property string mode: "quick"
    property string goal: "balanced"
    property var plan: ({})
    property var historyList: []
    // "setup" until a report is shown, either a fresh one or one from the history
    property bool showingReport: false
    // The run under way (BenchmarkRunner.sessionReady's label)
    property string currentLabel: ""
    // The picked result row (index in report.ranked): « Appliquer » / « Enregistrer comme profil » act on it
    property int sel: 0
    property string focusId: "bm0"

    readonly property real k: 1.25
    readonly property var report: BenchmarkRunner.report
    readonly property var ranked: report.ranked || []
    readonly property bool running: BenchmarkRunner.running
    readonly property bool done: !running && showingReport && ranked.length > 0
    readonly property bool setup: !running && !done
    readonly property color ink: Theme.ludique ? Theme.fg : Theme.accent

    readonly property var modes: [
        {id: "quick", name: qsTr("Rapide"), desc: qsTr("Tes réglages, les autres codecs, deux autres modes de régularité.")},
        {id: "full", name: qsTr("Complet"), desc: qsTr("Chaque codec à trois débits, puis chaque mode de régularité.")},
        {id: "adaptive", name: qsTr("Adaptatif"), desc: qsTr("Cherche le meilleur codec, puis son meilleur débit, puis la meilleure régularité, selon ton objectif.")}
    ]
    readonly property var goals: [
        {id: "balanced", name: qsTr("Équilibré")},
        {id: "latency", name: qsTr("Latence la plus basse")},
        {id: "smoothness", name: qsTr("Le plus fluide")}
    ]
    function modeIndex() { for (var i = 0; i < modes.length; i++) if (modes[i].id === mode) return i; return 0 }
    function goalIndex() { for (var i = 0; i < goals.length; i++) if (goals[i].id === goal) return i; return 0 }

    function refreshPlan() { plan = BenchmarkRunner.describe(ComputerManager, hostUuid, mode, goal) }
    function refreshHistory() { historyList = BenchmarkRunner.history(hostUuid) }

    // ---- results ------------------------------------------------------------------------------
    function isReference(r) { return r.role === "baseline" || r.role === "baseline-repeat" }
    readonly property real referenceScore: {
        var runs = report.runs || []
        for (var i = 0; i < runs.length; i++) if (runs[i].role === "baseline") return runs[i].goalScore
        return 0
    }
    function french(v, digits) { return Number(v).toFixed(digits).replace(".", ",") }
    function rowName(r) { return isReference(r) ? r.roleLabel : r.label }
    function rowDetail(r) {
        if (r.status !== "ok") {
            return (r.status === "skipped" ? qsTr("Sauté") : r.status === "fallback" ? qsTr("Pas testé comme demandé") : qsTr("Échec"))
                   + (r.note ? " : " + r.note : "")
        }
        var ms = qsTr("%1 ms estimées").arg(Math.round(r.metrics.totalMs))
        if (isReference(r)) return r.label + " · " + ms
        return ms + " · " + qsTr("%1 % des images affichées").arg(french(r.metrics.deliveredPct, 1))
    }
    function rowDelta(r) {
        if (isReference(r)) return qsTr("référence")
        if (r.status !== "ok" || !r.comparable) return ""
        var d = Math.round(r.goalScore - referenceScore)
        return (d > 0 ? "+" + d : d < 0 ? "−" + (-d) : "±0") + " " + qsTr("pts")
    }
    readonly property var selRun: ranked.length > 0 ? ranked[Math.min(sel, ranked.length - 1)] : null
    readonly property bool canApply: !!selRun && selRun.status === "ok" && selRun.role === "candidate" && report.applicable === true

    // ---- keys and focus (prototype ids: bm*, bg*, bgo, bstop, br*, bapply, bsave, bback) -------
    readonly property var grid: {
        var rows = [["back"]]
        if (setup) {
            rows.push(["bm0", "bm1", "bm2"], ["bg0", "bg1", "bg2"], ["bgo"])
            for (var i = 0; i < historyList.length; i++) rows.push(["h" + i + "o", "h" + i + "d"])
            rows.push(["bimport"])
        } else if (running) {
            rows.push(["bstop"])
        } else {
            for (var j = 0; j < ranked.length; j++) rows.push(["br" + j])
            rows.push(["bapply", "bsave", "bback"])
        }
        return rows
    }
    function defaultFocus() { return running ? "bstop" : (done ? "br0" : "bm" + modeIndex()) }
    onRunningChanged: focusId = defaultFocus()
    onDoneChanged: focusId = defaultFocus()
    function move(dx, dy) {
        var r = -1, c = 0
        for (var i = 0; i < grid.length; i++) { var j = grid[i].indexOf(focusId); if (j >= 0) { r = i; c = j; break } }
        if (r < 0) { focusId = defaultFocus(); return }
        if (dy !== 0) {
            r = Math.max(0, Math.min(grid.length - 1, r + dy))
            c = Math.min(c, grid[r].length - 1)
        } else {
            c = Math.max(0, Math.min(grid[r].length - 1, c + dx))
        }
        focusId = grid[r][c]
        if (/^br\d+$/.test(focusId)) {
            sel = parseInt(focusId.substring(2))
            results.ensureShown(sel)
        }
    }
    function act(id) {
        if (id === "back") return goBack()
        if (/^bm\d$/.test(id)) { mode = modes[parseInt(id.substring(2))].id; refreshPlan(); return }
        if (/^bg\d$/.test(id)) { goal = goals[parseInt(id.substring(2))].id; refreshPlan(); return }
        if (id === "bgo") { if (plan.available === true) BenchmarkRunner.start(ComputerManager, hostUuid, mode, goal); return }
        if (id === "bstop") return BenchmarkRunner.cancel()
        if (id === "bimport") return importDialog.open()
        var h = /^h(\d+)([od])$/.exec(id)
        if (h) {
            var item = historyList[parseInt(h[1])]
            if (!item) return
            if (h[2] === "o") {
                if (BenchmarkRunner.openReport(item.id)) { sel = 0; showingReport = true }
            } else {
                BenchmarkRunner.deleteReport(item.id)
                refreshHistory()
                focusId = "bgo"
            }
            return
        }
        if (/^br\d+$/.test(id)) { sel = parseInt(id.substring(2)); return }
        if (id === "bapply") { if (canApply) BenchmarkRunner.applyRun(selRun.index); return }
        if (id === "bsave") { if (canApply) BenchmarkRunner.applyRunAsProfile(selRun.index, qsTr("Benchmark : %1").arg(selRun.label)); return }
        if (id === "bback") { showingReport = false; refreshPlan(); refreshHistory(); return }
    }
    function goBack() {
        if (done) { showingReport = false; refreshPlan(); refreshHistory(); return }
        stackView.pop()
    }
    function mouseAt(id) { Theme.input = "mouse"; focusId = id }

    Keys.onPressed: (event) => {
        var key = event.key
        if (key === Qt.Key_Left || key === Qt.Key_Right || key === Qt.Key_Up || key === Qt.Key_Down) {
            Theme.input = "keys"
            move(key === Qt.Key_Left ? -1 : (key === Qt.Key_Right ? 1 : 0), key === Qt.Key_Up ? -1 : (key === Qt.Key_Down ? 1 : 0))
            event.accepted = true
        } else if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Space) {
            Theme.input = "keys"
            act(focusId)
            event.accepted = true
        } else if (key === Qt.Key_Escape || key === Qt.Key_Backspace) {
            if (running) BenchmarkRunner.cancel()
            else goBack()
            event.accepted = true
        }
    }

    Component.onCompleted: {
        refreshPlan()
        refreshHistory()
        focusId = defaultFocus()
    }
    // Leaving the page ends the benchmark after the current run
    Component.onDestruction: BenchmarkRunner.cancel()
    StackView.onActivated: {
        refreshPlan()
        refreshHistory()
        forceActiveFocus()
    }

    Connections {
        target: BenchmarkRunner
        function onSessionReady(session, label) {
            page.currentLabel = label
            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {"appName": label, "session": session, "benchmarkMode": true,
                                                           "hostName": page.hostName})
            stackView.push(segue)
        }
        function onFinished() {
            // Stopped before any run: nothing to show, the setup page (and its message) comes back
            var runs = BenchmarkRunner.report.runs
            page.sel = 0
            page.showingReport = runs !== undefined && runs.length > 0
            page.currentLabel = ""
            page.refreshPlan()
            page.refreshHistory()
            page.focusId = page.defaultFocus()
        }
    }

    // A button of the page: « sel » (chosen segment: accent), « off » (segment), « surf » (surface + border),
    // « main » (accent), « ink » (Ouvrir: fg on bg)
    component Btn: Rectangle {
        id: btn
        property string fid: ""
        property string label: ""
        property string look: "surf"
        property real fontSize: 15
        property int weight: Font.Bold
        property bool play: false
        property bool active: true
        property real pad: 22 * page.k
        readonly property bool focused: Theme.showFocus && page.focusId === fid
        implicitWidth: row.implicitWidth + 2 * pad
        implicitHeight: 48 * page.k
        width: implicitWidth
        height: implicitHeight
        radius: look === "sel" || look === "off" ? Theme.rs * page.k : Math.min(height / 2, Theme.pill * page.k)
        opacity: active ? 1 : 0.45
        enabled: active
        color: focused && Theme.ringFill ? Theme.ringFillColor
             : look === "sel" || look === "main" ? Theme.accent
             : look === "off" ? "transparent"
             : look === "ink" ? Theme.fg
             : look === "surf2" ? Theme.surface2 : Theme.surface
        border.width: look === "surf" || look === "surf2" ? Theme.border * page.k : 0
        border.color: Theme.borderColor
        readonly property color textColor: focused && Theme.ringFill ? Theme.ringFillFg
                                         : look === "sel" || look === "main" ? Theme.onAccent
                                         : look === "ink" ? Theme.bg : Theme.fg
        // The label inside the MouseArea: a click on the text is the button's (test channel: not « covered »)
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: { page.mouseAt(btn.fid); if (btn.active) page.act(btn.fid) }
            Row {
                id: row
                anchors.centerIn: parent
                spacing: 10 * page.k
                LavIcon {
                    visible: btn.play
                    name: "play"
                    filled: true
                    size: 16 * page.k
                    color: btn.textColor
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: btn.label
                    color: btn.textColor
                    font.family: Theme.textFont
                    font.weight: btn.weight
                    font.pixelSize: btn.fontSize * page.k
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
        FocusRing { k: page.k; active: btn.focused && !Theme.ringFill; radius: btn.radius }
    }
    // Test / Objectif: the segments in a surface2 track (pad 3, gap 3)
    component Segments: Rectangle {
        id: seg
        property var items: []
        property string prefix: ""
        property int current: 0
        width: segRow.width + 6 * page.k
        height: segRow.height + 6 * page.k
        radius: Theme.rs * page.k
        color: Theme.surface2
        Row {
            id: segRow
            x: 3 * page.k; y: 3 * page.k
            spacing: 3 * page.k
            Repeater {
                model: seg.items
                delegate: Btn {
                    required property var modelData
                    required property int index
                    fid: seg.prefix + index
                    label: modelData.name
                    look: index === seg.current ? "sel" : "off"
                    fontSize: 14
                    implicitHeight: 40 * page.k
                    pad: 16 * page.k
                    width: Math.max(120 * page.k, implicitWidth)
                }
            }
        }
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }
    Stage {
        id: stage
        anchors.fill: parent
        readonly property real w: width / stage.k
        readonly property real h: height / stage.k

        // ---- header ------------------------------------------------------------------------------
        Item {
            x: 40 * page.k; y: 24 * page.k
            width: stage.w - 80 * page.k
            height: 48 * page.k
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16 * page.k
                Rectangle {
                    id: backBtn
                    width: 44 * page.k; height: width
                    radius: Math.min(width / 2, Theme.pill * page.k)
                    color: Theme.showFocus && page.focusId === "back" && Theme.ringFill ? Theme.ringFillColor : Theme.surface
                    border.width: Theme.border * page.k
                    border.color: Theme.borderColor
                    anchors.verticalCenter: parent.verticalCenter
                    LavIcon { anchors.centerIn: parent; name: "back"; size: 20 * page.k; color: Theme.fg }
                    FocusRing { k: page.k; active: Theme.showFocus && page.focusId === "back" && !Theme.ringFill; radius: backBtn.radius }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { page.mouseAt("back"); page.goBack() } }
                }
                Text {
                    text: qsTr("Benchmark des réglages")
                    color: Theme.fg
                    font.family: Theme.displayFont
                    font.weight: Theme.displayWeight
                    font.pixelSize: 34 * page.k
                    font.letterSpacing: Theme.tracking * 34 * page.k
                    font.capitalization: Theme.upper ? Font.AllUppercase : Font.MixedCase
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: page.hostName + (page.hostSoft ? " · " + page.hostSoft : "")
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 14 * page.k
            }
        }

        // ---- setup (V8-43) -----------------------------------------------------------------------------
        Column {
            id: setupCol
            visible: page.setup
            x: 40 * page.k; y: 100 * page.k
            width: 740 * page.k
            spacing: 16 * page.k
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                text: qsTr("Streame plusieurs fois de suite une scène animée depuis %1, chaque fois avec un autre codec, un autre débit ou une autre régularité des images, puis note la latence et la fluidité de chaque essai face à tes réglages actuels. Tes réglages ne changent que si tu appliques un résultat.").arg(page.hostName)
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 15 * page.k
            }
            Column {
                spacing: 8 * page.k
                Text { text: qsTr("Test"); color: Theme.fg; font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 15 * page.k }
                Segments { items: page.modes; prefix: "bm"; current: page.modeIndex() }
                Text { text: page.modes[page.modeIndex()].desc; color: Theme.muted; font.family: Theme.textFont; font.pixelSize: 13 * page.k }
            }
            Column {
                spacing: 8 * page.k
                Text { text: qsTr("Objectif"); color: Theme.fg; font.family: Theme.textFont; font.weight: Font.Bold; font.pixelSize: 15 * page.k }
                Segments { items: page.goals; prefix: "bg"; current: page.goalIndex() }
            }
            Rectangle {
                visible: summary.text !== ""
                width: parent.width
                height: summary.implicitHeight + 28 * page.k
                radius: Theme.rs * page.k
                color: Theme.surface
                border.width: Theme.border * page.k
                border.color: Theme.borderColor
                Text {
                    id: summary
                    x: 16 * page.k; y: 14 * page.k
                    width: parent.width - 32 * page.k
                    wrapMode: Text.WordWrap
                    lineHeight: 1.25
                    color: Theme.fg
                    font.family: Theme.textFont
                    font.pixelSize: 14 * page.k
                    text: page.plan.available === true
                          ? qsTr("Tes réglages : %1. Environ %2 essais, %3 minutes. N’utilise pas le PC pendant le test : tout ce qui bouge à l’écran ou sur le réseau change les résultats.")
                                .arg(page.plan.baseline || "").arg(page.plan.runs || 0).arg(page.plan.minutes || 0)
                          : (page.plan.reason || "")
                }
            }
            // Warning: an app runs on the host; then the runner's message (stopped, nothing measured…)
            Repeater {
                model: [page.plan.otherAppRunning === true ? qsTr("Une appli tourne sur %1 : elle sera fermée.").arg(page.hostName) : "",
                        BenchmarkRunner.message]
                delegate: Rectangle {
                    required property var modelData
                    visible: modelData !== ""
                    width: setupCol.width
                    height: warnText.implicitHeight + 24 * page.k
                    radius: Theme.rs * page.k
                    color: Theme.warnBg
                    Rectangle { width: 4 * page.k; height: parent.height; color: Theme.warnLine }
                    Text {
                        id: warnText
                        x: 16 * page.k; y: 12 * page.k
                        width: parent.width - 32 * page.k
                        wrapMode: Text.WordWrap
                        text: modelData
                        color: Theme.fg
                        font.family: Theme.textFont
                        font.weight: Font.DemiBold
                        font.pixelSize: 14 * page.k
                    }
                }
            }
            Btn {
                fid: "bgo"
                label: qsTr("Lancer le benchmark")
                look: "main"
                play: true
                active: page.plan.available === true
                implicitHeight: 54 * page.k
                pad: 30 * page.k
                fontSize: 17
                weight: Font.ExtraBold
            }
        }
        // « Benchmarks précédents » (and opening a report file: one made on another device)
        Column {
            visible: page.setup
            width: 400 * page.k
            x: stage.w - 40 * page.k - width; y: 100 * page.k
            spacing: 10 * page.k
            Text {
                text: qsTr("Benchmarks précédents")
                color: Theme.fg
                font.family: Theme.displayFont
                font.weight: Theme.displayWeight
                font.pixelSize: 20 * page.k
                font.letterSpacing: Theme.tracking * 20 * page.k
                font.capitalization: Theme.upper ? Font.AllUppercase : Font.MixedCase
            }
            Repeater {
                model: page.historyList
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: 400 * page.k
                    height: histCol.implicitHeight + 28 * page.k
                    radius: Theme.rs * page.k
                    color: Theme.surface
                    border.width: Theme.border * page.k
                    border.color: Theme.borderColor
                    Column {
                        id: histCol
                        x: 16 * page.k; y: 14 * page.k
                        width: parent.width - 32 * page.k
                        spacing: 6 * page.k
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: modelData.createdAt + " · " + modelData.modeLabel + " · " + modelData.goalLabel
                                  + (modelData.platform === "android" ? " · Android" : "")
                            color: Theme.fg
                            font.family: Theme.textFont
                            font.weight: Font.Bold
                            font.pixelSize: 15 * page.k
                        }
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            visible: text !== ""
                            text: [modelData.best ? qsTr("Meilleur : %1").arg(modelData.best) : "",
                                   modelData.cancelled ? qsTr("arrêté") : "",
                                   modelData.unstable ? qsTr("instable") : ""].filter(function(t) { return t !== "" }).join(" · ")
                            color: Theme.muted
                            font.family: Theme.textFont
                            font.pixelSize: 13 * page.k
                        }
                        Row {
                            spacing: 8 * page.k
                            topPadding: 4 * page.k
                            Btn { fid: "h" + index + "o"; label: qsTr("Ouvrir"); look: "ink"; fontSize: 13; implicitHeight: 32 * page.k; pad: 14 * page.k }
                            Btn { fid: "h" + index + "d"; label: qsTr("Supprimer"); look: "surf2"; fontSize: 13; implicitHeight: 32 * page.k; pad: 14 * page.k }
                        }
                    }
                }
            }
            Btn { fid: "bimport"; label: qsTr("Ouvrir un rapport…"); look: "surf"; fontSize: 13; implicitHeight: 32 * page.k; pad: 14 * page.k }
        }

        // ---- running (V8-44) -------------------------------------------------------------------------
        Column {
            visible: page.running
            width: 720 * page.k
            x: (stage.w - width) / 2
            y: (stage.h - height) / 2
            spacing: 16 * page.k
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8 * page.k
                LiveDot { k: page.k; color: page.ink; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: BenchmarkRunner.runEstimate > 0
                          ? qsTr("Essai %1 sur %2").arg(Math.max(1, Math.min(BenchmarkRunner.runNumber, BenchmarkRunner.runEstimate))).arg(BenchmarkRunner.runEstimate)
                          : qsTr("Préparation")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.weight: Font.Bold
                    font.pixelSize: 14 * page.k
                    font.letterSpacing: 0.06 * 14 * page.k
                    font.capitalization: Font.AllUppercase
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: page.currentLabel !== "" ? page.currentLabel : BenchmarkRunner.message
                color: Theme.fg
                font.family: Theme.displayFont
                font.weight: Theme.displayWeight
                font.pixelSize: 40 * page.k
                font.letterSpacing: Theme.tracking * 40 * page.k
                font.capitalization: Theme.upper ? Font.AllUppercase : Font.MixedCase
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 520 * page.k
                height: 10 * page.k
                radius: Math.min(height / 2, Theme.pill * page.k)
                color: Theme.off
                clip: true
                Rectangle {
                    height: parent.height
                    radius: parent.radius
                    color: Theme.on
                    width: BenchmarkRunner.runEstimate > 0
                           ? parent.width * Math.min(1, Math.max(0, BenchmarkRunner.runNumber - 1) / BenchmarkRunner.runEstimate) : 0
                    Behavior on width { NumberAnimation { duration: 300 } }
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 560 * page.k
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                text: qsTr("La scène de test tourne sur %1. Ne touche à rien pendant la mesure : tout ce qui bouge à l’écran ou sur le réseau change les résultats.").arg(page.hostName)
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 15 * page.k
            }
            Item { width: 1; height: 8 * page.k }
            Btn {
                anchors.horizontalCenter: parent.horizontalCenter
                fid: "bstop"
                label: qsTr("Arrêter")
                look: "surf"
                pad: 24 * page.k
            }
        }

        // ---- results (V8-45) -------------------------------------------------------------------------
        Item {
            visible: page.done
            x: 40 * page.k; y: 92 * page.k
            width: stage.w - 80 * page.k
            height: stage.h - y - 56 * page.k
            Column {
                id: doneHead
                width: parent.width
                spacing: 6 * page.k
                Text {
                    text: (page.report.modeLabel || "") + " · " + qsTr("objectif %1").arg(String(page.report.goalLabel || "").toLowerCase())
                          + " · " + (page.report.hostName || "") + " · " + (page.report.createdAt || "")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 14 * page.k
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    visible: page.report.unstable === true || page.report.applicable === false
                    text: page.report.applicable === false
                          ? qsTr("Ce rapport a été fait sur Android : ses réglages ne s’appliquent pas ici.")
                          : qsTr("Tes réglages ont obtenu %1 points d’écart entre le début et la fin : les conditions ont changé pendant le benchmark, les petites différences ne veulent pas dire grand-chose. Relance-le.").arg(page.french(page.report.baselineDrift || 0, 1))
                    color: Theme.fg
                    font.family: Theme.textFont
                    font.weight: Font.DemiBold
                    font.pixelSize: 13 * page.k
                }
            }
            Flickable {
                id: results
                y: doneHead.height + 8 * page.k
                width: parent.width
                height: Math.max(0, Math.min(contentHeight, parent.height - y - legend.height - actions.height - 30 * page.k))
                contentHeight: resultsCol.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                function ensureShown(i) {
                    var top = i * 66 * page.k, bottom = top + 58 * page.k
                    if (top < contentY) contentY = top
                    else if (bottom > contentY + height) contentY = bottom - height
                }
                Column {
                    id: resultsCol
                    width: results.width
                    spacing: 8 * page.k
                    Repeater {
                        model: page.ranked
                        delegate: Rectangle {
                            id: resRow
                            required property var modelData
                            required property int index
                            readonly property bool focused: Theme.showFocus && page.focusId === "br" + index
                            readonly property bool ok: modelData.status === "ok"
                            readonly property color fgc: focused && Theme.ringFill ? Theme.ringFillFg : Theme.fg
                            readonly property color mutedc: focused && Theme.ringFill ? Theme.ringFillFg : Theme.muted
                            width: resultsCol.width
                            height: 58 * page.k
                            radius: Theme.rs * page.k
                            color: focused && Theme.ringFill ? Theme.ringFillColor : (index === page.sel ? Theme.surface2 : Theme.surface)
                            border.width: Theme.border * page.k
                            border.color: Theme.borderColor
                            Row {
                                x: 18 * page.k
                                width: parent.width - 36 * page.k
                                height: parent.height
                                spacing: 18 * page.k
                                Text {
                                    width: 26 * page.k
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: String(resRow.index + 1)
                                    color: resRow.fgc
                                    font.family: Theme.displayFont
                                    font.weight: Theme.displayWeight
                                    font.pixelSize: 20 * page.k
                                }
                                Column {
                                    width: parent.width - (26 + 150 + 150 + 120 + 4 * 18) * page.k
                                    anchors.verticalCenter: parent.verticalCenter
                                    Row {
                                        spacing: 8 * page.k
                                        width: parent.width
                                        Text {
                                            text: page.rowName(resRow.modelData)
                                            width: Math.min(implicitWidth, parent.width - (bestBadge.visible ? bestBadge.width + 8 * page.k : 0))
                                            elide: Text.ElideRight
                                            color: resRow.fgc
                                            font.family: Theme.textFont
                                            font.weight: Font.Bold
                                            font.pixelSize: 15 * page.k
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Rectangle {
                                            id: bestBadge
                                            visible: resRow.modelData.index === page.report.bestIndex
                                            width: bestText.implicitWidth + 16 * page.k
                                            height: bestText.implicitHeight + 4 * page.k
                                            radius: Math.min(height / 2, Theme.pill * page.k)
                                            color: Theme.badgeBg
                                            anchors.verticalCenter: parent.verticalCenter
                                            Text {
                                                id: bestText
                                                anchors.centerIn: parent
                                                text: qsTr("MEILLEUR")
                                                color: Theme.badgeFg
                                                font.family: Theme.textFont
                                                font.weight: Font.ExtraBold
                                                font.pixelSize: 11 * page.k
                                            }
                                        }
                                    }
                                    Text {
                                        width: parent.width
                                        elide: Text.ElideRight
                                        text: page.rowDetail(resRow.modelData)
                                        color: resRow.mutedc
                                        font.family: Theme.textFont
                                        font.pixelSize: 12.5 * page.k
                                    }
                                }
                                Text {
                                    width: 150 * page.k
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: resRow.ok
                                    textFormat: Text.StyledText
                                    text: qsTr("Latence %1").arg(Math.round(resRow.modelData.latency))
                                          + " <font color='" + resRow.mutedc + "'>±" + Math.round(resRow.modelData.spreadLatency) + "</font>"
                                    color: resRow.fgc
                                    font.family: Theme.textFont
                                    font.weight: Font.Bold
                                    font.pixelSize: 14 * page.k
                                }
                                Text {
                                    width: 150 * page.k
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: resRow.ok
                                    textFormat: Text.StyledText
                                    text: qsTr("Fluidité %1").arg(Math.round(resRow.modelData.smoothness))
                                          + " <font color='" + resRow.mutedc + "'>±" + Math.round(resRow.modelData.spreadSmoothness) + "</font>"
                                    color: resRow.fgc
                                    font.family: Theme.textFont
                                    font.weight: Font.Bold
                                    font.pixelSize: 14 * page.k
                                }
                                Text {
                                    readonly property string delta: page.rowDelta(resRow.modelData)
                                    width: 120 * page.k
                                    anchors.verticalCenter: parent.verticalCenter
                                    horizontalAlignment: Text.AlignRight
                                    text: delta
                                    color: delta.charAt(0) === "+" && !(resRow.focused && Theme.ringFill) ? page.ink : resRow.mutedc
                                    font.family: Theme.textFont
                                    font.weight: Font.Bold
                                    font.pixelSize: 14 * page.k
                                }
                            }
                            FocusRing { k: page.k; active: resRow.focused && !Theme.ringFill; radius: resRow.radius }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { page.mouseAt("br" + resRow.index); page.sel = resRow.index }
                            }
                        }
                    }
                }
            }
            Text {
                id: legend
                y: results.y + results.height + 12 * page.k
                width: parent.width
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                text: qsTr("Latence et fluidité sont notées sur 100, classées selon ton objectif. Le dernier chiffre compare chaque essai à tes réglages ; ± dit de combien la note a bougé pendant l’essai.")
                      + (BenchmarkRunner.message !== "" ? "\n" + BenchmarkRunner.message : "")
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 13 * page.k
            }
            Row {
                id: actions
                anchors.bottom: parent.bottom
                spacing: 10 * page.k
                Btn { fid: "bapply"; label: qsTr("Appliquer à mes réglages"); look: "main"; active: page.canApply }
                Btn { fid: "bsave"; label: qsTr("Enregistrer comme profil"); look: "surf"; active: page.canApply }
                Btn { fid: "bback"; label: qsTr("Retour"); look: "surf" }
            }
        }

        HintsBar {
            k: page.k
            hints: [{k: "A", l: qsTr("Valider"), act: function() { page.act(page.focusId) }},
                    {k: "B", l: qsTr("Retour"), act: function() { if (page.running) BenchmarkRunner.cancel(); else page.goBack() }}]
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Ouvrir un rapport de benchmark")
        nameFilters: [qsTr("Rapport de benchmark (*.json)")]
        onAccepted: {
            if (BenchmarkRunner.importReport(selectedFile)) {
                page.sel = 0
                page.showingReport = true
                page.refreshHistory()
            }
        }
    }
}
