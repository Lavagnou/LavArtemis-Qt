import QtQuick

// Empty Playnite library on the home (prototype noGames): dashed muted frame on the surface,
// « Bibliothèque Playnite » in the display font, then the LavApollo Link hint.
Item {
    id: ng
    property real k: 1.25
    property real topMargin: 0
    DashedRect {
        y: ng.topMargin
        width: parent.width
        height: parent.height - ng.topMargin
        radius: Theme.r * ng.k
        borderWidth: 2 * ng.k
        borderColor: Theme.muted
        fillColor: Theme.surface
    }
    Column {
        x: 28 * ng.k
        y: ng.topMargin + (parent.height - ng.topMargin - height) / 2
        width: Math.min(parent.width - 56 * ng.k, 760 * ng.k)
        spacing: 8 * ng.k
        Text {
            text: Theme.displayText(qsTr("Bibliothèque Playnite"))
            color: Theme.fg
            font.family: Theme.displayFont
            font.weight: Theme.displayWeight
            font.pixelSize: 24 * ng.k
            font.letterSpacing: Theme.tracking * 24 * ng.k
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Aucun jeu Playnite sur ce PC. Installe l’extension LavApollo Link dans Playnite pour voir ta bibliothèque ici.")
            color: Theme.muted
            lineHeight: 1.5
            font.family: Theme.textFont
            font.pixelSize: 15 * ng.k
        }
    }
}
