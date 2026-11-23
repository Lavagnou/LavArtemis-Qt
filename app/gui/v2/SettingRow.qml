import QtQuick

import ArtemisSettings 1.0

// One row of Réglages v2: a label, a short description underneath (muted) and the control on the
// right. `expert` rows only show in expert mode; `available` hides a row the platform or another
// setting makes pointless. The row itself takes the focus (Tab, arrows, gamepad): Return/Space
// emit activate(), Left/Right go to stepHandler(delta) when the row has one -- it returns true
// when it changed something, otherwise the key goes on to the page (Left goes back to the pages).
// A row whose control holds several buttons sets `focusable: false`: the buttons take the focus.
Item {
    id: row

    property string label: ""
    property string description: ""
    property int descriptionLines: 2
    property bool expert: false
    property bool available: true
    property bool focusable: true
    property bool clickToActivate: false
    property var stepHandler: null
    default property alias control: slot.data

    readonly property bool shown: available && (!expert || ArtemisSettings.uiExpertMode)

    signal activate()

    visible: shown
    width: parent ? parent.width : 0
    implicitHeight: Math.max(texts.implicitHeight, slot.height) + 34
    height: implicitHeight
    activeFocusOnTab: focusable
    opacity: enabled ? 1.0 : 0.45

    Keys.onPressed: (event) => {
        if (Theme.input !== "keys") Theme.input = "keys"
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            row.activate()
            event.accepted = true
        } else if ((event.key === Qt.Key_Left || event.key === Qt.Key_Right) && row.stepHandler) {
            event.accepted = row.stepHandler(event.key === Qt.Key_Right ? 1 : -1) === true
        }
    }

    HoverHandler { id: hover }
    TapHandler {
        onTapped: {
            if (row.focusable) row.forceActiveFocus(Qt.MouseFocusReason)
            if (row.clickToActivate) row.activate()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.rs
        color: Theme.surface
        border.width: Math.max(1, Theme.border)
        border.color: hover.hovered && Theme.input === "mouse" && row.enabled && (row.clickToActivate || row.focusable)
                      ? Qt.alpha(Theme.accent, 0.6) : Theme.borderColor
        Behavior on border.color { ColorAnimation { duration: Theme.animMs } }
    }

    // Focus ring: gamepad and keyboard only
    Rectangle {
        anchors.fill: parent
        anchors.margins: -5
        radius: Theme.rs + 5
        color: "transparent"
        border.width: 3
        border.color: Theme.fg
        visible: row.activeFocus && Theme.showFocus
    }

    Column {
        id: texts
        x: 24
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(120, row.width - 48 - (slot.width > 0 ? slot.width + 28 : 0))
        spacing: 5

        Row {
            spacing: 10
            width: parent.width
            Text {
                id: labelText
                width: Math.min(implicitWidth, texts.width - (tag.visible ? tag.width + 10 : 0))
                text: row.label
                color: Theme.fg
                elide: Text.ElideRight
                font.family: Theme.textFont
                font.pixelSize: 20
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                id: tag
                visible: row.expert
                width: tagText.implicitWidth + 14
                height: 22
                radius: Theme.id === "affiche" ? 0 : 11
                color: "transparent"
                border.width: 1
                border.color: Theme.muted
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    id: tagText
                    anchors.centerIn: parent
                    text: qsTr("EXPERT")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 0.8
                }
            }
        }

        Text {
            width: parent.width
            visible: text.length > 0
            text: row.description
            color: Theme.muted
            wrapMode: Text.Wrap
            maximumLineCount: row.descriptionLines
            elide: Text.ElideRight
            font.family: Theme.textFont
            font.pixelSize: 16
        }
    }

    Item {
        id: slot
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        width: childrenRect.width
        height: childrenRect.height
    }
}
