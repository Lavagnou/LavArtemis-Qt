import QtQuick
import QtQuick.Controls

// Choice row of Réglages v2: segmented buttons for a few short options, a themed dropdown beyond
// `segmentMax`. Left/Right step through the segments (options marked `ask`, which open a dialog,
// are skipped), Return moves to the next one or opens the dropdown. chosen(value) asks the page to
// write the value; `value` stays bound to the preference.
SettingRow {
    id: choice

    property var options: []        // [{text, value, ask}]
    property var value
    property int segmentMax: 4
    property bool dropdown: options.length > segmentMax
    property real dropdownWidth: 380
    naturalControlWidth: dropdown ? dropdownWidth : segments.width

    readonly property int currentIndex: indexOf(value)

    signal chosen(var value)

    function indexOf(v) {
        for (var i = 0; i < options.length; i++) {
            if (options[i].value === v) return i
        }
        return -1
    }

    stepHandler: dropdown ? null : function(delta) {
        var i = currentIndex
        do {
            i += delta
        } while (i >= 0 && i < options.length && options[i].ask === true)
        if (i < 0 || i >= options.length || i === currentIndex) return false
        chosen(options[i].value)
        return true
    }

    onActivate: {
        if (!enabled || options.length === 0) return
        if (dropdown) list.openList()
        else chosen(options[(currentIndex + 1) % options.length].value)
    }

    Item {
        width: choice.dropdown ? list.width : segments.width
        height: choice.dropdown ? list.height : segments.height

        LavDropdown {
            id: list
            visible: choice.dropdown
            width: choice.stacked ? choice.stackedControlWidth : choice.dropdownWidth
            focusPolicy: Qt.NoFocus
            options: choice.options
            currentIndex: choice.currentIndex
            onClicked: choice.forceActiveFocus(Qt.MouseFocusReason)
            onChosen: (index) => choice.chosen(choice.options[index].value)
        }

        Rectangle {
            id: segments
            visible: !choice.dropdown
            width: segmentRow.width + 8
            height: 50
            radius: Theme.pill
            color: Theme.surface2
            border.width: Theme.id === "affiche" ? 2 : 0
            border.color: Theme.fg

            Row {
                id: segmentRow
                x: 4
                y: 4
                spacing: 4
                Repeater {
                    model: choice.options
                    delegate: AbstractButton {
                        id: segment
                        required property var modelData
                        required property int index
                        readonly property bool selected: index === choice.currentIndex
                        focusPolicy: Qt.NoFocus
                        hoverEnabled: true
                        height: 42
                        width: Math.max(64, segmentLabel.implicitWidth + 30)
                        onClicked: {
                            choice.forceActiveFocus(Qt.MouseFocusReason)
                            choice.chosen(modelData.value)
                        }
                        background: Rectangle {
                            radius: Theme.id === "affiche" ? 0 : Math.max(0, Theme.pill - 4)
                            color: segment.selected ? Theme.accent
                                 : (segment.hovered && Theme.input === "mouse" ? Theme.surface : "transparent")
                            Behavior on color { ColorAnimation { duration: Theme.animMs } }
                        }
                        contentItem: Text {
                            id: segmentLabel
                            text: segment.modelData.text
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: segment.selected ? Theme.onAccent : Theme.fg
                            font.family: Theme.textFont
                            font.pixelSize: 16
                            font.weight: segment.selected ? Font.Bold : Font.DemiBold
                        }
                    }
                }
            }
        }
    }
}
