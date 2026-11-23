import QtQuick
import QtQuick.Controls

// Centred, modal dialog of the redesigned UI: title, text, optional fields, then the buttons.
// Fields go in `fields: [...]`. Escape or a click outside counts as a refusal (rejected()).
LavPopup {
    id: dialog

    property string title: ""
    property string text: ""
    property string acceptText: qsTr("OK")
    property string rejectText: qsTr("Annuler")
    property bool acceptEnabled: true
    property Item initialFocus: null
    property alias fields: fieldsColumn.data

    property bool _accepted: false

    signal accepted()
    signal rejected()

    function accept() {
        if (!acceptEnabled) return
        _accepted = true
        close()
        accepted()
    }

    modal: true
    designWidth: 640
    anchors.centerIn: Overlay.overlay

    Overlay.modal: Rectangle { color: Qt.alpha("#000000", 0.45) }

    onOpened: {
        _accepted = false
        if (initialFocus) initialFocus.forceActiveFocus()
        else acceptButton.forceActiveFocus()
    }
    onClosed: if (!_accepted) rejected()

    Text {
        width: dialog.bodyWidth
        visible: dialog.title.length > 0
        text: Theme.displayText(dialog.title)
        wrapMode: Text.Wrap
        color: Theme.fg
        font.family: Theme.displayFont
        font.pixelSize: 28
        font.weight: Theme.displayWeight
        font.letterSpacing: Theme.tracking * 28
    }

    Text {
        width: dialog.bodyWidth
        visible: dialog.text.length > 0
        text: dialog.text
        wrapMode: Text.Wrap
        color: Theme.muted
        font.family: Theme.textFont
        font.pixelSize: 17
        lineHeight: 1.15
    }

    Column {
        id: fieldsColumn
        width: dialog.bodyWidth
        spacing: 14
        visible: children.length > 0
    }

    Row {
        spacing: 14
        topPadding: 6
        LavButton {
            id: acceptButton
            text: dialog.acceptText
            enabled: dialog.acceptEnabled
            opacity: enabled ? 1.0 : 0.4
            onClicked: dialog.accept()
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
        }
        LavButton {
            visible: dialog.rejectText.length > 0
            primary: false
            text: dialog.rejectText
            onClicked: dialog.close()
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
        }
    }
}
