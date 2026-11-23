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

    signal moved(real value)

    function snap(v) {
        v = Math.max(from, Math.min(to, v))
        if (stepSize > 0) v = from + Math.round((v - from) / stepSize) * stepSize
        return Math.max(from, Math.min(to, v))
    }

    stepHandler: function(delta) {
        var v = snap(value + delta * keyStep)
        if (v === value) return false
        moved(v)
        return true
    }

    Row {
        spacing: 20

        Item {
            id: track
            width: sl.sliderWidth
            height: 40
            anchors.verticalCenter: parent.verticalCenter
            readonly property real position: sl.to > sl.from
                                             ? Math.max(0, Math.min(1, (sl.value - sl.from) / (sl.to - sl.from))) : 0

            Rectangle {
                id: groove
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
                    var p = Math.max(0, Math.min(1, (mx - groove.x) / groove.width))
                    return sl.snap(sl.from + p * (sl.to - sl.from))
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
