import QtQuick

// On/off row of Réglages v2: the whole row is clickable, Return/Space flip it. `checked` is bound
// to the preference by the page; toggled(value) asks the page to write it.
SettingRow {
    id: toggle

    property bool checked: false

    signal toggled(bool value)

    clickToActivate: true
    onActivate: if (enabled) toggled(!checked)

    LavSwitch { on: toggle.checked }
}
