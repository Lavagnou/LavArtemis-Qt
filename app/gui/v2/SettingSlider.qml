import QtQuick

// Slider row of Réglages v2, drawn in the theme. The track always shows `value` (bound to the
// preference by the page): a drag or Left/Right emit moved(value) and the page writes it back, so
// a change made elsewhere (profile switch, calibration) is never hidden by a stale slider.
SettingRow {
    id: sl

    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real keyStep: stepSize
    property real value: 0
    property string valueText: String(value)
    property real sliderWidth: 380
    // 1 = linear. Above 1 the track gives more room to low values (the bitrate uses 3, as
    // Android's SettingDef.curve): position = fraction^(1/curve), and Left/Right steps grow with
    // the value (×5 past a sixteenth of the range, ×20 past a quarter), as SettingDef.stepFrom.
    property real curve: 1
    naturalControlWidth: sliderWidth + 20 + 150

    signal moved(real value)

    function snap(v) {
        v = Math.max(from, Math.min(to, v))
        if (stepSize > 0) v = from + Math.round((v - from) / stepSize) * stepSize
        return Math.max(from, Math.min(to, v))
    }

    function positionOf(v) {
        if (to <= from) return 0
        var p = Math.max(0, Math.min(1, (v - from) / (to - from)))
        return curve === 1 ? p : Math.pow(p, 1 / curve)
    }

    function valueAtPosition(p) {
        p = Math.max(0, Math.min(1, p))
        if (curve !== 1) p = Math.pow(p, curve)
        return snap(Math.round(from + p * (to - from)))
    }

    stepHandler: function(delta) {
        var s = keyStep
        if (curve !== 1) {
            var span = to - from
            if (value >= from + span / 4) s = keyStep * 20
            else if (value >= from + span / 16) s = keyStep * 5
        }
        var v = snap(value + delta * s)
        if (v === value) return false
        moved(v)
        return true
    }

    Row {
        spacing: 20

        Item {
            id: track
            width: sl.stacked ? sl.stackedControlWidth - 20 - 150 : sl.sliderWidth
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            readonly property real position: sl.positionOf(sl.value)

            Rectangle {
                id: groove
                // Test build: the bench clicks at a fraction of it (the track's MouseArea takes it)
                objectName: "sliderGroove"
                x: 12
                width: parent.width - 24
                height: 8
                radius: Theme.id === "affiche" ? 0 : 4
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.off
                Rectangle {
                    width: groove.width * track.position
                    height: parent.height
                    radius: parent.radius
                    color: Theme.on
                }
            }
            Rectangle {
                width: 26
                height: 26
                radius: Theme.id === "affiche" ? 0 : 13
                x: groove.x + groove.width * track.position - width / 2
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.fg
                border.width: 3
                border.color: Theme.surface
                scale: dragArea.pressed ? 1.15 : 1.0
                Behavior on scale { NumberAnimation { duration: Theme.animMs } }
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                enabled: sl.enabled
                preventStealing: true
                function valueAt(mx) {
                    return sl.valueAtPosition((mx - groove.x) / groove.width)
                }
                function update(mx) {
                    var v = valueAt(mx)
                    if (v !== sl.value) sl.moved(v)
                }
                onPressed: (mouse) => {
                    if (sl.focusable) sl.forceActiveFocus(Qt.MouseFocusReason)
                    update(mouse.x)
                }
                onPositionChanged: (mouse) => { if (pressed) update(mouse.x) }
            }
        }

        Text {
            width: 150
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            text: sl.valueText
            color: Theme.fg
            font.family: Theme.displayFont
            font.pixelSize: 20
            font.weight: Font.Bold
        }
    }
}
