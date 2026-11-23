import QtQuick
import QtQuick.Controls

// Options of an app or game. With the mouse: a compact popover glued to the pointer. With a
// gamepad or the keyboard: the same menu, centred, with a dimmed background.
Popup {
    id: menu

    property string appName: ""
    property url cover: ""
    property bool running: false
    property bool canHide: true
    property real atX: 0
    property real atY: 0

    signal triggered(string action)

    readonly property bool anchored: Theme.input === "mouse" || Theme.input === "touch"

    function openAt(px, py) {
        atX = px
        atY = py
        open()
    }

    modal: true
    focus: true
    padding: 10
    width: 320
    x: anchored ? Math.max(12, Math.min(atX, parent.width - width - 12)) : (parent.width - width) / 2
    y: anchored ? Math.max(12, Math.min(atY, parent.height - height - 12)) : (parent.height - height) / 2

    Overlay.modal: Rectangle {
        // Light veil: the home stays visible behind the menu
        color: Qt.alpha("#000000", menu.anchored ? 0.08 : 0.35)
    }

    background: Rectangle {
        radius: Theme.id === "affiche" ? 0 : (Theme.id === "ludique" ? 24 : 16)
        color: Theme.surface
        border.width: Math.max(1, Theme.border)
        border.color: Theme.borderColor
    }

    readonly property var entries: {
        var list = []
        list.push({id: "launch", label: running ? qsTr("Reprendre") : qsTr("Lancer"), icon: "play", main: true})
        if (running) {
            list.push({id: "quit", label: qsTr("Quitter"), icon: "stop"})
            list.push({id: "relaunch", label: qsTr("Relancer la session"), icon: "refresh"})
        }
        list.push({id: "art", label: qsTr("Créer un raccourci .art"), icon: "link"})
        if (canHide) {
            list.push({id: "hide", label: qsTr("Masquer"), icon: "eye-off"})
        }
        return list
    }

    contentItem: Column {
        spacing: 4

        Row {
            spacing: 12
            leftPadding: 6
            bottomPadding: 6
            Image {
                width: 36; height: 48
                source: menu.cover
                fillMode: Image.PreserveAspectCrop
                visible: status === Image.Ready
            }
            Text {
                width: 240
                text: menu.appName
                color: Theme.fg
                elide: Text.ElideRight
                font.family: Theme.displayFont
                font.pixelSize: 18
                font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Repeater {
            model: menu.entries
            delegate: AbstractButton {
                id: row
                required property var modelData
                required property int index
                width: menu.width - menu.padding * 2
                height: 46
                hoverEnabled: true
                focus: index === 0
                KeyNavigation.down: null
                Keys.onDownPressed: nextItemInFocusChain(true).forceActiveFocus()
                Keys.onUpPressed: nextItemInFocusChain(false).forceActiveFocus()
                onClicked: {
                    menu.close()
                    menu.triggered(modelData.id)
                }
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()

                background: Rectangle {
                    radius: Theme.id === "affiche" ? 0 : (Theme.id === "ludique" ? 16 : 10)
                    color: row.modelData.main ? Theme.accent
                         : ((row.hovered || (row.activeFocus && Theme.showFocus)) ? Theme.sel : "transparent")
                }
                contentItem: Row {
                    spacing: 12
                    leftPadding: 12
                    LavIcon {
                        name: row.modelData.icon
                        size: 18
                        color: row.modelData.main ? Theme.onAccent : Theme.fg
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: row.modelData.label
                        color: row.modelData.main ? Theme.onAccent
                             : ((row.hovered || row.activeFocus) && !row.modelData.main ? Theme.selFg : Theme.fg)
                        font.family: Theme.textFont
                        font.pixelSize: 17
                        font.weight: row.modelData.main ? Font.Bold : Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }
}
