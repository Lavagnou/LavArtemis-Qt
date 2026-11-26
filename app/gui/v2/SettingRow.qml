import QtQuick

import ArtemisSettings 1.0

// One row of Réglages v2: a label, a short description underneath (muted) and the control on the
// right. `expert` rows only show in expert mode; `available` hides a row the platform or another
// setting makes pointless. The row itself takes the focus (Tab, arrows, gamepad): Return/Space
// emit activate(), Left/Right go to stepHandler(delta) when the row has one -- it returns true
// when it changed something, otherwise the key goes on to the page (Left goes back to the pages).
// A row whose control holds several buttons sets `focusable: false`: the buttons take the focus.
// Narrow window (prototype V6, piste A): when the control would leave the text less than
// minTextWidth, it goes under the label and the description; a control that can stretch (dropdown,
// slider) then takes the row's width (stackedControlWidth). Such a control declares its width when
// side by side in naturalControlWidth, so that `stacked` never depends on the stretched width.
Item {
    id: row

    property string label: ""
    property string description: ""
    property int descriptionLines: 2
    property bool expert: false
    // « ● » after the label (prototype V8 « Suit le profil »): rows of a page whose settings are saved in the
    // active profile (the page Column says profileRows), except the ones kept by this PC (appWide)
    property bool appWide: false
    readonly property bool followsProfile: !appWide && parent !== null && parent.profileRows === true
    property bool available: true
    property bool focusable: true
    property bool clickToActivate: false
    property var stepHandler: null
    default property alias control: slot.data
    property real naturalControlWidth: slot.width
    property real minTextWidth: 320
    readonly property bool stacked: naturalControlWidth > 0 && width - 48 - naturalControlWidth - 28 < minTextWidth
    readonly property real stackedControlWidth: width - 48

    readonly property bool shown: available && (!expert || ArtemisSettings.uiExpertMode)

    signal activate()

    visible: shown
    width: parent ? parent.width : 0
    implicitHeight: stacked ? texts.implicitHeight + 14 + slot.height + 34
                            : Math.max(texts.implicitHeight, slot.height) + 34
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

    // Focus ring: gamepad and keyboard only, the theme's (prototype ring)
    FocusRing {
        active: row.activeFocus && Theme.showFocus
        radius: Theme.rs
        k: 1.25
    }

    Column {
        id: texts
        x: 24
        y: row.stacked ? 17 : (row.height - implicitHeight) / 2
        width: row.stacked ? row.width - 48 : Math.max(120, row.width - 48 - (slot.width > 0 ? slot.width + 28 : 0))
        spacing: 5

        Row {
            spacing: 10
            width: parent.width
            Text {
                id: labelText
                width: Math.min(implicitWidth, texts.width - (tag.visible ? tag.width + 10 : 0) - (profileDot.visible ? profileDot.width + 10 : 0))
                text: row.label
                color: Theme.fg
                elide: Text.ElideRight
                font.family: Theme.textFont
                font.pixelSize: 20
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: profileDot
                visible: row.followsProfile
                text: "●"
                color: Theme.muted
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                // the prototype's chip: surface2, radius 6, 10 px bold, letter-spaced
                id: tag
                visible: row.expert
                width: tagText.implicitWidth + 15
                height: tagText.implicitHeight + 3
                radius: Theme.id === "affiche" ? 0 : 7.5
                color: Theme.surface2
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    id: tagText
                    anchors.centerIn: parent
                    text: qsTr("EXPERT")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 12
                    font.weight: Font.ExtraBold
                    font.letterSpacing: 0.5
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
        x: row.stacked ? 24 : row.width - 24 - width
        y: row.stacked ? 17 + texts.implicitHeight + 14 : (row.height - height) / 2
        width: childrenRect.width
        height: childrenRect.height
    }
}
