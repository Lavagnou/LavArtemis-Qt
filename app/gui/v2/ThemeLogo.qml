import QtQuick
import SystemProperties 1.0

// The app logo. LavArtemis Dev always shows its own icon (the « plan d'atelier »: blue squared
// background, white drawing, green DEV badge; decision of 2026-11-26), whatever the theme. The
// stable build shows the logo of the theme (prototype §1.15): Braise orange tile, Affiche black
// tile (yellow in dark), Ludique navy. The files come from LavArtemis/docs/branding: never
// redraw them here.
Image {
    property real size: 34
    width: size
    height: size
    sourceSize.width: 2 * size
    sourceSize.height: 2 * size
    source: SystemProperties.isDevChannel ? "qrc:/res/lavartemis-dev.svg"
          : Theme.braise ? "qrc:/res/lavartemis-braise.svg"
          : Theme.affiche ? (Theme.dark ? "qrc:/res/lavartemis-affiche-dark.svg" : "qrc:/res/lavartemis-affiche-light.svg")
          : "qrc:/res/lavartemis.svg"
}
