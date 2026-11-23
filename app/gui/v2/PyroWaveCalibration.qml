import QtQuick
import QtQuick.Controls

import SystemProperties 1.0
import PyroWaveCalibrator 1.0

// PyroWave calibration results (Réglages v2, Vidéo): one cell per resolution and chroma, SDR only
// (HDR is not offered by the redesigned settings). Picking a cell emits applied(option); the page
// writes the codec, resolution, chroma and bitrate. Same data and wording as the dialog of the
// legacy settings screen, without its HDR cells (HDR was removed).
LavPopup {
    id: dialog

    property int testFps: 60
    property string detail: ""

    signal applied(var option)

    modal: true
    designWidth: 940
    anchors.centerIn: Overlay.overlay
    Overlay.modal: Rectangle { color: Qt.alpha("#000000", 0.45) }

    // Finished results stay; the format being tested is abandoned
    onAboutToHide: PyroWaveCalibrator.cancel()
    onOpened: {
        detail = ""
        closeButton.forceActiveFocus()
    }

    readonly property var rows: (Qt.platform.os === "linux" ? [
        { name: "4K", width: 3840, height: 2160 },
        { name: "1440p", width: 2560, height: 1440 },
        { name: "1080p", width: 1920, height: 1080 },
        { name: "800p", width: 1280, height: 800 },
        { name: "720p", width: 1280, height: 720 }
    ] : [
        { name: "4K", width: 3840, height: 2160 },
        { name: "1440p", width: 2560, height: 1440 },
        { name: "1080p", width: 1920, height: 1080 },
        { name: "720p", width: 1280, height: 720 }
    ])
    readonly property var samples: PyroWaveCalibrator.results

    // Results arrive in sweep order: per resolution, 4:4:4 HDR, 4:4:4 SDR, 4:2:0 HDR, 4:2:0 SDR
    function sample(row, mode) {
        var offset = row * 4 + mode
        return offset < samples.length ? samples[offset] : null
    }

    function canApply(option) {
        return option && option.valid && !option.hdr
    }

    function mbps(kbps) {
        return String(Math.round(kbps / 100) / 10).replace(".", ",")
    }

    function headline(option) {
        if (option && option.valid) return qsTr("%1 Mbit/s").arg(mbps(option.bitrateKbps))
        return "SDR"
    }

    function tierText(option) {
        if (!option) return PyroWaveCalibrator.running ? qsTr("Test en cours…") : "—"
        if (option.tier === "error") return option.error
        if (option.tier === "slow") return qsTr("Trop lent")
        var text = option.tier === "any" ? qsTr("Fluide") :
                   option.tier === "vrr" ? qsTr("Juste") : qsTr("Très juste")
        if (option.quality === "reduced") text += " · " + qsTr("qualité réduite")
        else if (option.quality === "low") text += " · " + qsTr("basse qualité")
        return text
    }

    function tierColor(option) {
        if (!option) return Theme.muted
        return Theme.tierColors[option.tier]
    }

    function optionDetail(option) {
        if (!option || !option.valid) return ""
        var cost = qsTr("99 % des images se décodent et s'affichent en %1 ms au plus, soit %2 % de chaque image à %3 FPS.")
                       .arg(option.frameMs.toFixed(1).replace(".", ",")).arg(option.loadPercent).arg(testFps)
        if (!option.keepsUp) {
            return cost + " " + qsTr("Baisser le débit ne suffit pas : le stream saccadera ou prendra du retard. Tu peux quand même le choisir.")
        }
        var details = [cost]
        if (option.tier === "vrr") {
            details.push(qsTr("Une image lente peut sortir un rafraîchissement trop tard, surtout sur un écran à fréquence fixe."))
        } else if (option.tier === "vrrLarge") {
            details.push(qsTr("Les images lentes prennent presque toute la période : attends-toi à des saccades, surtout sur un écran à fréquence fixe."))
        }
        details.push(qsTr("%1 Mbit/s donnent %2 dB sur l'échelle de qualité de l'auteur du codec, qui recommande %3 Mbit/s (35 dB) pour ce format.")
                     .arg(mbps(option.bitrateKbps)).arg(option.qualityDb.toFixed(1).replace(".", ","))
                     .arg(mbps(option.guideKbps)))
        if (option.deviceLimited) details.push(qsTr("Le débit a été baissé pour que ce PC suive."))
        else if (option.linkLimited) details.push(qsTr("Le débit est plafonné par la carte réseau de ce PC."))
        return details.join(" ")
    }

    Text {
        width: dialog.bodyWidth
        text: Theme.displayText(qsTr("Calibrage PyroWave — %1 FPS").arg(dialog.testFps))
        wrapMode: Text.Wrap
        color: Theme.fg
        font.family: Theme.displayFont
        font.pixelSize: 28
        font.weight: Theme.displayWeight
    }

    Text {
        width: dialog.bodyWidth
        visible: text.length > 0
        text: PyroWaveCalibrator.message
        wrapMode: Text.Wrap
        color: Theme.fg
        font.family: Theme.textFont
        font.pixelSize: 17
    }

    Text {
        width: dialog.bodyWidth
        text: PyroWaveCalibrator.linkSummary + " " +
              qsTr("Le test de débit est un transfert en bloc ; un vrai stream peut encore subir des pertes. Choisir un format applique le débit affiché.")
        wrapMode: Text.Wrap
        color: Theme.muted
        font.family: Theme.textFont
        font.pixelSize: 15
    }

    Column {
        width: dialog.bodyWidth
        spacing: 6
        Repeater {
            model: [
                { tier: "any", text: qsTr("Fluide : 99 % des images prennent au plus 60 % de leur temps, de la marge pour le travail en plus d'un vrai stream.") },
                { tier: "vrr", text: qsTr("Juste : les images lentes prennent jusqu'à 80 % de leur temps. Une image peut sortir en retard ; un écran VRR le cache mieux.") },
                { tier: "vrrLarge", text: qsTr("Très juste : les images lentes prennent presque tout leur temps. Quelques saccades à prévoir.") },
                { tier: "slow", text: qsTr("Trop lent : à %1 FPS, plus d'une image sur 100 dépasse son temps. Saccades à prévoir ; reste possible.").arg(dialog.testFps) }
            ]
            delegate: Row {
                required property var modelData
                spacing: 10
                Rectangle {
                    width: 12; height: 12; radius: 6
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.tierColors[parent.modelData.tier]
                }
                Text {
                    width: dialog.bodyWidth - 22
                    text: parent.modelData.text
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.family: Theme.textFont
                    font.pixelSize: 14
                }
            }
        }
    }

    Row {
        spacing: 10
        Text {
            width: 110
            text: qsTr("Résolution")
            color: Theme.muted
            font.family: Theme.textFont
            font.pixelSize: 15
            font.weight: Font.Bold
        }
        Repeater {
            model: ["4:4:4", "4:2:0"]
            delegate: Text {
                required property string modelData
                width: (dialog.bodyWidth - 130) / 2
                horizontalAlignment: Text.AlignHCenter
                text: modelData
                color: Theme.muted
                font.family: Theme.textFont
                font.pixelSize: 15
                font.weight: Font.Bold
            }
        }
    }

    Repeater {
        model: dialog.rows
        delegate: Row {
            id: calibrationRow
            required property var modelData
            required property int index
            readonly property bool hgroup: true
            spacing: 10

            Text {
                width: 110
                anchors.verticalCenter: parent.verticalCenter
                text: calibrationRow.modelData.name
                color: Theme.fg
                font.family: Theme.textFont
                font.pixelSize: 18
                font.weight: Font.Bold
            }

            Repeater {
                // 4:4:4 SDR then 4:2:0 SDR
                model: [1, 3]
                delegate: AbstractButton {
                    id: cell
                    required property int modelData
                    readonly property var option: dialog.sample(calibrationRow.index, modelData)
                    width: (dialog.bodyWidth - 130) / 2
                    height: 72
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    enabled: dialog.canApply(option)
                    onClicked: {
                        dialog.applied(option)
                        dialog.close()
                    }
                    Keys.onReturnPressed: clicked()
                    Keys.onEnterPressed: clicked()
                    onHoveredChanged: if (hovered) dialog.detail = dialog.optionDetail(option)
                    onActiveFocusChanged: if (activeFocus) dialog.detail = dialog.optionDetail(option)

                    background: Rectangle {
                        radius: Theme.rs
                        color: cell.hovered && cell.enabled ? Theme.sel : Theme.surface2
                        border.width: cell.activeFocus && Theme.showFocus ? 3 : 1
                        border.color: cell.activeFocus && Theme.showFocus ? Theme.fg : Theme.borderColor
                    }
                    contentItem: Column {
                        spacing: 4
                        opacity: cell.enabled ? 1.0 : 0.6
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            text: dialog.headline(cell.option)
                            color: cell.hovered && cell.enabled ? Theme.selFg : Theme.fg
                            font.family: Theme.textFont
                            font.pixelSize: 17
                            font.weight: Font.Bold
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 8
                            Rectangle {
                                width: 10; height: 10; radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                visible: !!cell.option
                                color: dialog.tierColor(cell.option)
                            }
                            Text {
                                text: dialog.tierText(cell.option)
                                color: cell.hovered && cell.enabled ? Theme.selFg : Theme.muted
                                font.family: Theme.textFont
                                font.pixelSize: 14
                            }
                        }
                    }
                }
            }
        }
    }

    Text {
        width: dialog.bodyWidth
        text: dialog.detail.length > 0 ? dialog.detail
              : qsTr("Chaque format est testé au débit recommandé par l'auteur du codec. « Qualité réduite » ou « basse qualité » : il a fallu baisser le débit pour que ce PC suive.")
        wrapMode: Text.Wrap
        color: Theme.muted
        font.family: Theme.textFont
        font.pixelSize: 15
    }

    LavButton {
        id: closeButton
        primary: false
        text: qsTr("Fermer")
        onClicked: dialog.close()
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
    }
}
