import QtQuick
import QtQuick.Controls

import ArtemisSettings 1.0

// Réglages of the redesigned UI, phase 1: Apparence lives here (theme, light/dark, way back to
// the legacy interface); every other setting still opens the existing settings screen until the
// expert-mode reorganisation lands (spec, "Réglages").
FocusScope {
    id: settingsPage
    objectName: qsTr("Réglages")
    focus: true
    readonly property bool isV2Page: true

    HoverHandler {
        onPointChanged: if (Theme.input !== "mouse" && point.device.type === PointerDevice.Mouse) Theme.input = "mouse"
    }
    Keys.onPressed: (event) => {
        if (Theme.input !== "keys") Theme.input = "keys"
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) { stackView.pop(); event.accepted = true }
    }

    Rectangle { anchors.fill: parent; color: Theme.bg }

    Stage {
        anchors.fill: parent

        Flickable {
            anchors.fill: parent
            contentHeight: column.implicitHeight + 60
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: column
                x: 50; y: 24
                width: parent.width - 100
                spacing: 26

                PageHeader { title: qsTr("Réglages"); onBack: stackView.pop() }

                SectionTitle { text: qsTr("Apparence") }

                Text {
                    text: qsTr("Thème")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 17
                }
                Row {
                    spacing: 18
                    Repeater {
                        model: ["braise", "affiche", "ludique"]
                        delegate: AbstractButton {
                            id: themeCard
                            required property string modelData
                            readonly property var pal: Theme.palettes[modelData][Theme.dark ? "dark" : "light"]
                            readonly property bool selected: Theme.id === modelData
                            width: 220; height: 132
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus
                            onClicked: Theme.setTheme(modelData)
                            Keys.onReturnPressed: clicked()
                            background: Rectangle {
                                radius: themeCard.modelData === "affiche" ? 0 : (themeCard.modelData === "ludique" ? 24 : 16)
                                color: themeCard.pal.bg
                                border.width: themeCard.selected || (themeCard.activeFocus && Theme.showFocus) ? 4 : 1
                                border.color: themeCard.selected ? Theme.accent : (themeCard.activeFocus ? Theme.fg : Theme.line)
                                Row {
                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 16
                                    spacing: 8
                                    Repeater {
                                        model: [themeCard.pal.accent, themeCard.pal.surface2, themeCard.pal.fg]
                                        delegate: Rectangle {
                                            required property var modelData
                                            width: 22; height: 22
                                            radius: themeCard.modelData === "affiche" ? 0 : 11
                                            color: modelData
                                            border.width: 1
                                            border.color: themeCard.pal.line
                                        }
                                    }
                                }
                            }
                            contentItem: Item {
                                Text {
                                    x: 16; y: 14
                                    text: themeCard.modelData === "affiche" ? Theme.themeNames[themeCard.modelData].toUpperCase()
                                                                            : Theme.themeNames[themeCard.modelData]
                                    color: themeCard.pal.fg
                                    font.pixelSize: 26
                                    font.weight: Font.Black
                                    font.family: themeCard.modelData === "affiche" ? "Archivo"
                                               : (themeCard.modelData === "ludique" ? "Bricolage Grotesque" : "Space Grotesk")
                                }
                            }
                        }
                    }
                }

                Text {
                    text: qsTr("Clair ou sombre")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 17
                }
                Row {
                    spacing: 12
                    Repeater {
                        model: 3
                        delegate: LavButton {
                            required property int index
                            primary: Theme.mode === index
                            text: Theme.modeNames[index]
                            iconName: index === 0 ? "" : (index === 1 ? "sun" : "moon")
                            onClicked: {
                                ArtemisSettings.uiThemeMode = index
                                ArtemisSettings.save()
                            }
                        }
                    }
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("« Système » suit le mode clair ou sombre de Windows, en direct.")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 15
                }

                SectionTitle { text: qsTr("Interface") }
                Row {
                    spacing: 16
                    LavButton {
                        primary: false
                        text: qsTr("Revenir à l'ancienne interface")
                        onClicked: legacyDialog.open()
                    }
                }

                SectionTitle { text: qsTr("Stream, manettes, PC…") }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("Les autres réglages s'ouvrent encore dans l'écran actuel, le temps de leur nouvelle organisation (avec le mode expert).")
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 15
                }
                LavButton {
                    text: qsTr("Tous les réglages")
                    iconName: "gear"
                    onClicked: {
                        var component = Qt.createComponent("../SettingsView.qml")
                        stackView.push(component.createObject(stackView, {}))
                    }
                }
            }
        }
    }

    Dialog {
        id: legacyDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Ancienne interface")
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: 420
            wrapMode: Text.Wrap
            text: qsTr("LavArtemis Dev rouvrira l'ancienne interface au prochain démarrage. Tu pourras revenir à la nouvelle depuis Réglages › LavArtemis Features.")
        }
        onAccepted: {
            ArtemisSettings.uiLegacy = true
            ArtemisSettings.save()
        }
    }
}
