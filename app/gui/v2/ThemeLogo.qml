import QtQuick

// The app logo of the theme (prototype §1.15): Braise orange tile, Affiche black tile (yellow in
// dark), Ludique navy. The Dev build says so with the DEV chip next to the wordmark.
Image {
    property real size: 34
    width: size
    height: size
    sourceSize.width: 2 * size
    sourceSize.height: 2 * size
    source: Theme.braise ? "qrc:/res/lavartemis-braise.svg"
          : Theme.affiche ? (Theme.dark ? "qrc:/res/lavartemis-affiche-dark.svg" : "qrc:/res/lavartemis-affiche-light.svg")
          : "qrc:/res/lavartemis.svg"
}
