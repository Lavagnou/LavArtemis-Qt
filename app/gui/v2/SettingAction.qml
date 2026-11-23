import QtQuick

// Row of Réglages v2 that runs something (check for updates, export, open the legacy screen):
// one button, Return/Space on the row press it.
SettingRow {
    id: action

    property string buttonText: ""
    property bool primary: false
    property bool busy: false

    signal triggered()

    onActivate: if (enabled && !busy) triggered()

    LavButton {
        text: action.buttonText
        primary: action.primary
        focusPolicy: Qt.NoFocus
        enabled: !action.busy
        opacity: enabled ? 1.0 : 0.5
        implicitHeight: 48
        onClicked: {
            action.forceActiveFocus(Qt.MouseFocusReason)
            action.triggered()
        }
    }
}
