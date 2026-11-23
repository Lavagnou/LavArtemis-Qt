import QtQuick

// On/off switch of the redesigned UI, drawn like the one inside LavPill. Visual only: the row
// that holds it (SettingToggle) takes the click and the keys.
Rectangle {
    id: sw

    property bool on: false

    width: 58
    height: 32
    radius: Theme.id === "affiche" ? 0 : height / 2
    color: on ? Theme.on : Theme.off
    border.width: Theme.id === "affiche" ? 2 : 0
    border.color: Theme.fg
    Behavior on color { ColorAnimation { duration: Theme.animMs } }

    Rectangle {
        width: 24
        height: 24
        y: 4
        x: sw.on ? sw.width - width - 4 : 4
        radius: Theme.id === "affiche" ? 0 : width / 2
        color: sw.on ? Theme.onAccent : Theme.fg
        Behavior on x { NumberAnimation { duration: Theme.animMs } }
    }
}
