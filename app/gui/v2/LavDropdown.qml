import QtQuick
import QtQuick.Controls

// Compact list choice of the redesigned UI: a framed button showing the current option, opening a
// themed list under it (no stock ComboBox look). Keyboard and gamepad: Return opens, Up/Down move,
// Return picks, Escape closes.
AbstractButton {
    id: dd

    property var options: []        // [{text, value}]
    property int currentIndex: -1
    property real listMaxHeight: 460

    signal chosen(int index)

    function openList() {
        if (options.length === 0) return
        // The list lives in the window overlay (unscaled), right under the button
        var p = dd.mapToItem(null, 0, dd.height + 6)
        list.x = p.x
        list.y = p.y
        list.open()
    }

    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    implicitWidth: 360
    implicitHeight: 50
    opacity: enabled ? 1.0 : 0.45

    onClicked: openList()
    Keys.onReturnPressed: openList()
    Keys.onEnterPressed: openList()

    background: Rectangle {
        radius: Theme.pill
        color: dd.down || list.opened ? Theme.surface : Theme.surface2
        border.width: dd.activeFocus && Theme.showFocus ? 3 : Math.max(1, Theme.border)
        border.color: dd.activeFocus && Theme.showFocus ? Theme.fg
                    : (dd.hovered && Theme.input === "mouse" ? Qt.alpha(Theme.accent, 0.6) : Theme.borderColor)
        Behavior on border.color { ColorAnimation { duration: Theme.animMs } }
    }

    contentItem: Item {
        Text {
            x: 18
            width: parent.width - 62
            anchors.verticalCenter: parent.verticalCenter
            text: dd.currentIndex >= 0 && dd.currentIndex < dd.options.length ? dd.options[dd.currentIndex].text : "—"
            color: Theme.fg
            elide: Text.ElideRight
            font.family: Theme.textFont
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }
        LavIcon {
            name: "chevron-down"
            size: 18
            color: Theme.fg
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    LavPopup {
        id: list
        parent: Overlay.overlay
        designWidth: Math.max(dd.width, 300)
        innerPadding: 8
        modal: false

        onOpened: {
            lv.currentIndex = Math.max(0, dd.currentIndex)
            lv.positionViewAtIndex(lv.currentIndex, ListView.Contain)
            lv.forceActiveFocus()
        }

        ListView {
            id: lv
            width: list.bodyWidth
            height: Math.min(contentHeight, dd.listMaxHeight)
            model: dd.options
            clip: true
            focus: true
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationWraps: false
            ScrollBar.vertical: ScrollBar { visible: lv.contentHeight > lv.height }

            function pick(index) {
                if (index < 0) return
                list.close()
                dd.chosen(index)
            }

            Keys.onReturnPressed: pick(currentIndex)
            Keys.onEnterPressed: pick(currentIndex)
            Keys.onSpacePressed: pick(currentIndex)

            delegate: AbstractButton {
                id: option
                required property var modelData
                required property int index
                readonly property bool selected: index === dd.currentIndex
                readonly property bool highlighted: ListView.isCurrentItem && Theme.input !== "mouse" || hovered
                width: lv.width
                height: 46
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                onClicked: lv.pick(index)

                background: Rectangle {
                    radius: Theme.id === "affiche" ? 0 : (Theme.id === "ludique" ? 16 : 10)
                    color: option.selected ? Theme.accent : (option.highlighted ? Theme.sel : "transparent")
                }
                contentItem: Text {
                    leftPadding: 12
                    rightPadding: 12
                    text: option.modelData.text
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    color: option.selected ? Theme.onAccent : (option.highlighted ? Theme.selFg : Theme.fg)
                    font.family: Theme.textFont
                    font.pixelSize: 17
                    font.weight: option.selected ? Font.Bold : Font.Medium
                }
            }
        }
    }
}
