import QtQuick

// Text entry of the redesigned UI (profile name, custom resolution). Return emits accepted(),
// even when the text is empty, so the dialog decides what an empty field means.
Rectangle {
    id: field

    property alias text: input.text
    property alias validator: input.validator
    property alias acceptableInput: input.acceptableInput
    property alias maximumLength: input.maximumLength
    property alias input: input
    property string placeholderText: ""

    signal accepted()

    implicitWidth: 240
    implicitHeight: 54
    radius: Theme.rs
    color: Theme.surface2
    border.width: input.activeFocus ? 3 : Math.max(1, Theme.border)
    border.color: input.activeFocus ? Theme.fg : Theme.borderColor

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        verticalAlignment: TextInput.AlignVCenter
        clip: true
        activeFocusOnTab: true
        selectByMouse: true
        color: Theme.fg
        selectionColor: Theme.accent
        selectedTextColor: Theme.onAccent
        font.family: Theme.textFont
        font.pixelSize: 19
        Keys.onReturnPressed: field.accepted()
        Keys.onEnterPressed: field.accepted()
    }

    Text {
        anchors.fill: input
        verticalAlignment: Text.AlignVCenter
        visible: input.text.length === 0
        text: field.placeholderText
        color: Theme.muted
        font: input.font
        elide: Text.ElideRight
    }
}
