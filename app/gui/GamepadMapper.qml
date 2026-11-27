import QtQuick
import QtQuick.Controls

import SdlGamepadMapper 1.0
import SdlGamepadKeyNavigation 1.0
import "v2"

// « Configurer une manette » (Réglages › Manettes), laid out as the prototype's mapper (planches V8-46, 47):
// the devices on the left (the picked one is being edited), on the right the prompt bar (« Clique un contrôle,
// puis actionne-le sur ta manette. Clic droit : l’effacer. » / « Actionne … sur ta manette… » and « N à faire »),
// the schematic (ControllerDiagram) and the actions. The first device is picked when the page opens: picking
// loads its current layout, nothing is written before « Enregistrer ». Prototype px x k (1.25 on the stage).
FocusScope {
    id: mapperPage
    objectName: qsTr("Configurer une manette")
    focus: true

    property var devices: []
    // The device being edited (index in `devices`), -1 when none
    property int picked: -1
    // True while "map everything" is walking the whole controller, so that
    // finishing the queue can offer to save. A one-off click shouldn't.
    property bool walkthroughRunning: false
    property string focusId: "md0"

    readonly property real k: 1.25
    readonly property bool waiting: SdlGamepadMapper.targetElement !== ""
    readonly property color ink: Theme.ludique ? Theme.fg : Theme.accent

    function refreshDevices() {
        devices = SdlGamepadMapper.enumerateDevices()
        if (picked >= devices.length) picked = -1
        if (picked < 0 && devices.length > 0) pick(0)
    }
    function pick(i) {
        if (i < 0 || i >= devices.length) return
        walkthroughRunning = false
        SdlGamepadMapper.cancel()
        picked = i
        SdlGamepadMapper.startMapping(devices[i].index)
    }
    function stateText(d) {
        return d.guessed ? qsTr("Disposition devinée — peut-être fausse")
             : d.recognized ? qsTr("Reconnue") : qsTr("Non reconnue — inutilisable avant assignation")
    }
    function stateColor(d) { return d.guessed ? ink : (d.recognized ? Theme.muted : "#E5484D") }

    // The three actions: while a control is waited on « Arrêter », « Passer », else « Tout assigner dans l’ordre »,
    // « Tout effacer »; « Enregistrer » last
    readonly property var actions: waiting
        ? [{id: "ma0", l: qsTr("Arrêter")}, {id: "ma1", l: qsTr("Passer")}, {id: "ma2", l: qsTr("Enregistrer"), main: true}]
        : [{id: "ma0", l: qsTr("Tout assigner dans l’ordre")}, {id: "ma1", l: qsTr("Tout effacer")}, {id: "ma2", l: qsTr("Enregistrer"), main: true}]
    function actionEnabled(id) {
        if (!SdlGamepadMapper.mapping) return false
        if (id === "ma1" && !waiting) return SdlGamepadMapper.bindingCount > 0
        // Saving a mapping with nothing bound would replace a working guess with an empty entry
        if (id === "ma2") return SdlGamepadMapper.bindingCount > 0
        return true
    }
    function save() {
        if (SdlGamepadMapper.saveMapping()) {
            toast.show(qsTr("Disposition enregistrée"))
            var keep = picked
            devices = SdlGamepadMapper.enumerateDevices()
            picked = keep
        }
    }
    function act(id) {
        if (id === "back") return stackView.pop()
        if (/^md\d+$/.test(id)) return pick(parseInt(id.substring(2)))
        if (id === "mrefresh") {
            refreshDevices()
            return toast.show(devices.length === 1 ? qsTr("Manettes : 1 trouvée") : qsTr("Manettes : %1 trouvées").arg(devices.length))
        }
        if (!actionEnabled(id)) return
        if (id === "ma0") {
            if (waiting) { SdlGamepadMapper.cancelSelection(); walkthroughRunning = false }
            else { walkthroughRunning = true; SdlGamepadMapper.mapEverything() }
        } else if (id === "ma1") {
            if (waiting) SdlGamepadMapper.skipTarget()
            else SdlGamepadMapper.clearAll()
        } else if (id === "ma2") {
            save()
        }
    }
    readonly property var grid: {
        var rows = [["back"]]
        for (var i = 0; i < devices.length; i++) rows.push(["md" + i])
        rows.push(["mrefresh"], ["ma0", "ma1", "ma2"])
        return rows
    }
    function move(dx, dy) {
        var r = -1, c = 0
        for (var i = 0; i < grid.length; i++) { var j = grid[i].indexOf(focusId); if (j >= 0) { r = i; c = j; break } }
        if (r < 0) { focusId = devices.length > 0 ? "md0" : "mrefresh"; return }
        if (dy !== 0) { r = Math.max(0, Math.min(grid.length - 1, r + dy)); c = Math.min(c, grid[r].length - 1) }
        else c = Math.max(0, Math.min(grid[r].length - 1, c + dx))
        focusId = grid[r][c]
    }
    function mouseAt(id) { Theme.input = "mouse"; focusId = id }

    // Gamepad UI navigation reads the very same buttons we're trying to
    // record. Left enabled, every press would move the focus instead of being
    // captured.
    StackView.onActivated: {
        SdlGamepadKeyNavigation.disable()
        refreshDevices()
        forceActiveFocus()
    }
    StackView.onDeactivating: {
        SdlGamepadMapper.cancel()
        SdlGamepadKeyNavigation.enable()
    }
    Component.onDestruction: SdlGamepadMapper.cancel()

    // Escape means "stop waiting for a control" first and "leave the page"
    // second. Without consuming it, the StackView's own handler would pop the
    // view out from under a capture.
    Keys.onPressed: (event) => {
        var key = event.key
        if (key === Qt.Key_Escape || key === Qt.Key_Backspace) {
            if (waiting) {
                SdlGamepadMapper.cancelSelection()
                walkthroughRunning = false
            } else {
                stackView.pop()
            }
            event.accepted = true
        } else if (key === Qt.Key_Left || key === Qt.Key_Right || key === Qt.Key_Up || key === Qt.Key_Down) {
            Theme.input = "keys"
            move(key === Qt.Key_Left ? -1 : (key === Qt.Key_Right ? 1 : 0), key === Qt.Key_Up ? -1 : (key === Qt.Key_Down ? 1 : 0))
            event.accepted = true
        } else if (key === Qt.Key_Return || key === Qt.Key_Enter || key === Qt.Key_Space) {
            Theme.input = "keys"
            act(focusId)
            event.accepted = true
        }
    }

    Connections {
        target: SdlGamepadMapper
        function onQueueFinished() {
            if (mapperPage.walkthroughRunning) {
                mapperPage.walkthroughRunning = false
                savePrompt.showDialog({head: qsTr("Manette") + " · " + (mapperPage.picked >= 0 ? mapperPage.devices[mapperPage.picked].name : ""),
                                       title: qsTr("Tout est assigné"), info: true,
                                       note: qsTr("Tous les contrôles sont passés. Enregistrer cette disposition ?"),
                                       entries: [{l: qsTr("Enregistrer"), a: "save", main: true}, {l: qsTr("Annuler"), a: "cancel"}]})
            }
        }
    }

    // A pill button of the page (« Actualiser », the actions)
    component Btn: Rectangle {
        id: btn
        property string fid: ""
        property string label: ""
        property bool main: false
        property bool active: true
        property real fontSize: 15
        property real pad: 20 * mapperPage.k
        readonly property bool focused: Theme.showFocus && mapperPage.focusId === fid
        width: lbl.implicitWidth + 2 * pad
        height: 46 * mapperPage.k
        radius: Math.min(height / 2, Theme.pill * mapperPage.k)
        enabled: active
        opacity: active ? 1 : 0.45
        color: focused && Theme.ringFill ? Theme.ringFillColor : (main ? Theme.accent : Theme.surface)
        border.width: Theme.border * mapperPage.k
        border.color: Theme.borderColor
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: { mapperPage.mouseAt(btn.fid); mapperPage.act(btn.fid) }
            Text {
                id: lbl
                anchors.centerIn: parent
                text: btn.label
                color: btn.focused && Theme.ringFill ? Theme.ringFillFg : (btn.main ? Theme.onAccent : Theme.fg)
                font.family: Theme.textFont
                font.weight: Font.Bold
                font.pixelSize: btn.fontSize * mapperPage.k
            }
        }
        FocusRing { k: mapperPage.k; active: btn.focused && !Theme.ringFill; radius: btn.radius }
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }
    Stage {
        id: stage
        anchors.fill: parent
        readonly property real w: width / stage.k
        readonly property real h: height / stage.k

        // ---- header ------------------------------------------------------------------------------
        Item {
            x: 40 * mapperPage.k; y: 24 * mapperPage.k
            width: stage.w - 80 * mapperPage.k
            height: 48 * mapperPage.k
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16 * mapperPage.k
                Rectangle {
                    id: backBtn
                    width: 44 * mapperPage.k; height: width
                    radius: Math.min(width / 2, Theme.pill * mapperPage.k)
                    color: Theme.showFocus && mapperPage.focusId === "back" && Theme.ringFill ? Theme.ringFillColor : Theme.surface
                    border.width: Theme.border * mapperPage.k
                    border.color: Theme.borderColor
                    anchors.verticalCenter: parent.verticalCenter
                    LavIcon { anchors.centerIn: parent; name: "back"; size: 20 * mapperPage.k; color: Theme.fg }
                    FocusRing { k: mapperPage.k; active: Theme.showFocus && mapperPage.focusId === "back" && !Theme.ringFill; radius: backBtn.radius }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: stackView.pop() }
                }
                Text {
                    text: qsTr("Configurer une manette")
                    color: Theme.fg
                    font.family: Theme.displayFont
                    font.weight: Theme.displayWeight
                    font.pixelSize: 34 * mapperPage.k
                    font.letterSpacing: Theme.tracking * 34 * mapperPage.k
                    font.capitalization: Theme.upper ? Font.AllUppercase : Font.MixedCase
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Réglages › Manettes")
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 14 * mapperPage.k
            }
        }

        // ---- devices -------------------------------------------------------------------------------
        Column {
            x: 40 * mapperPage.k; y: 96 * mapperPage.k
            width: 330 * mapperPage.k
            spacing: 10 * mapperPage.k
            Text {
                width: parent.width
                bottomPadding: 4 * mapperPage.k
                wrapMode: Text.WordWrap
                lineHeight: 1.3
                text: qsTr("Choisis une manette pour apprendre sa disposition à LavArtemis. Une manette déjà reconnue n’en a besoin que si un bouton est mal placé.")
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 14 * mapperPage.k
            }
            Text {
                visible: mapperPage.devices.length === 0
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("Aucune manette ni aucun joystick trouvé.")
                color: Theme.fg
                font.family: Theme.textFont
                font.weight: Font.DemiBold
                font.pixelSize: 14 * mapperPage.k
            }
            Repeater {
                model: mapperPage.devices
                delegate: Rectangle {
                    id: card
                    required property var modelData
                    required property int index
                    readonly property bool isPicked: index === mapperPage.picked
                    readonly property bool focused: Theme.showFocus && mapperPage.focusId === "md" + index
                    readonly property color fgc: focused && Theme.ringFill ? Theme.ringFillFg : Theme.fg
                    width: parent.width
                    height: Math.max(74 * mapperPage.k, cardCol.implicitHeight + 20 * mapperPage.k)
                    radius: Theme.rs * mapperPage.k
                    color: focused && Theme.ringFill ? Theme.ringFillColor : (isPicked ? Theme.surface2 : Theme.surface)
                    border.width: isPicked ? 2 * mapperPage.k : Theme.border * mapperPage.k
                    border.color: isPicked ? mapperPage.ink : Theme.borderColor
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { mapperPage.mouseAt("md" + card.index); mapperPage.pick(card.index) }
                        Column {
                            id: cardCol
                            x: 14 * mapperPage.k
                            width: parent.width - 28 * mapperPage.k
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3 * mapperPage.k
                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                text: card.modelData.name
                                color: card.fgc
                                font.family: Theme.textFont
                                font.weight: Font.Bold
                                font.pixelSize: 15 * mapperPage.k
                            }
                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: mapperPage.stateText(card.modelData)
                                color: card.focused && Theme.ringFill ? Theme.ringFillFg : mapperPage.stateColor(card.modelData)
                                font.family: Theme.textFont
                                font.weight: Font.DemiBold
                                font.pixelSize: 12.5 * mapperPage.k
                            }
                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                text: qsTr("%1 axes, %2 boutons, %3 croix").arg(card.modelData.numAxes).arg(card.modelData.numButtons).arg(card.modelData.numHats)
                                color: card.fgc
                                opacity: 0.75
                                font.family: Theme.textFont
                                font.pixelSize: 12 * mapperPage.k
                            }
                            // Raw device activity, independent of any binding: if this stays empty while the
                            // controller is operated, the problem is the device or the driver, not the mapping
                            Text {
                                visible: card.isPicked && SdlGamepadMapper.rawActivity !== ""
                                width: parent.width
                                elide: Text.ElideRight
                                text: SdlGamepadMapper.rawActivity
                                color: card.fgc
                                opacity: 0.75
                                font.family: "Consolas"
                                font.pixelSize: 11 * mapperPage.k
                            }
                        }
                    }
                    FocusRing { k: mapperPage.k; active: card.focused && !Theme.ringFill; radius: card.radius }
                }
            }
            Btn { fid: "mrefresh"; label: qsTr("Actualiser"); fontSize: 13; height: 36 * mapperPage.k; pad: 16 * mapperPage.k }
        }

        // ---- the schematic -------------------------------------------------------------------------
        Item {
            id: section
            x: 410 * mapperPage.k; y: 96 * mapperPage.k
            width: stage.w - x - 40 * mapperPage.k
            height: stage.h - y - 60 * mapperPage.k

            Rectangle {
                id: prompt
                width: parent.width
                height: Math.max(44 * mapperPage.k, promptText.implicitHeight + 16 * mapperPage.k)
                radius: Theme.rs * mapperPage.k
                color: mapperPage.waiting ? Theme.warnBg : Theme.surface
                Row {
                    x: 16 * mapperPage.k
                    width: parent.width - 32 * mapperPage.k
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12 * mapperPage.k
                    LiveDot { visible: mapperPage.waiting; k: mapperPage.k; color: mapperPage.ink; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        id: promptText
                        width: parent.width - leftText.width - (mapperPage.waiting ? 20 * mapperPage.k : 0) - 12 * mapperPage.k
                        wrapMode: Text.WordWrap
                        text: mapperPage.waiting ? SdlGamepadMapper.targetPrompt
                              : (SdlGamepadMapper.mapping ? qsTr("Clique un contrôle, puis actionne-le sur ta manette. Clic droit : l’effacer.")
                                                          : qsTr("Choisis une manette à gauche."))
                        color: Theme.fg
                        font.family: Theme.textFont
                        font.weight: Font.DemiBold
                        font.pixelSize: 14 * mapperPage.k
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        id: leftText
                        visible: SdlGamepadMapper.queueRemaining > 0
                        text: qsTr("%1 à faire").arg(SdlGamepadMapper.queueRemaining)
                        color: Theme.muted
                        font.family: Theme.textFont
                        font.pixelSize: 13 * mapperPage.k
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            ControllerDiagram {
                id: diagram
                y: prompt.height + 12 * mapperPage.k
                width: parent.width
                height: actionsRow.y - y - 12 * mapperPage.k
                opacity: SdlGamepadMapper.mapping ? 1 : 0.5
                enabled: SdlGamepadMapper.mapping
                bindings: SdlGamepadMapper.bindings
                values: SdlGamepadMapper.elementValues
                target: SdlGamepadMapper.targetElement
                onSelectRequested: function(elements) {
                    // Picking a control by hand replaces the queue, so a walkthrough that was running is
                    // over -- otherwise finishing this one control would pop the "save?" prompt.
                    mapperPage.walkthroughRunning = false
                    SdlGamepadMapper.selectElements(elements)
                }
                onClearRequested: function(elements) {
                    for (var i = 0; i < elements.length; i++) SdlGamepadMapper.clearElement(elements[i])
                }
            }

            Row {
                id: actionsRow
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10 * mapperPage.k
                Repeater {
                    model: mapperPage.actions
                    delegate: Btn {
                        required property var modelData
                        fid: modelData.id
                        label: modelData.l
                        main: !!modelData.main
                        active: mapperPage.actionEnabled(modelData.id)
                    }
                }
            }
        }

        HintsBar {
            k: mapperPage.k
            hints: [{k: "A", l: qsTr("Valider"), act: function() { mapperPage.act(mapperPage.focusId) }},
                    {k: "B", l: qsTr("Retour"), act: function() { stackView.pop() }}]
        }
        LavToast { id: toast; k: mapperPage.k }
    }

    LavOptions {
        id: savePrompt
        onPicked: (a) => { if (a === "save") mapperPage.save() }
        returnFocusTo: mapperPage
    }
}
