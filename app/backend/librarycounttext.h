#pragma once

#include <QString>

// The game count of the library page (LibraryView: the line under the filters, and Affiche's header in
// capitals). Hidden games never count: `visible` is the number of games that are not hidden.
//   - empty list: « ATLAS-PC · aucun jeu »
//   - filter « Tous », or a list of every visible game: « ATLAS-PC · 265 jeux » (« 1 jeu »)
//   - otherwise: « ATLAS-PC · 12 affichés sur 265 » (« 1 affiché sur 265 »)
//   - filter « Masqués » (`hiddenFilter`): the hidden games it lists, « ATLAS-PC · 3 jeux masqués »
//     (« 1 jeu masqué », « aucun jeu masqué »), never « N affichés sur M »: M counts no hidden game
// Pure function, tested by tests/librarycount; shown through SystemProperties.libraryCountText().
namespace LibraryCountText {

QString line(const QString& hostName, int shown, int visible, bool allFilter, bool hiddenFilter = false);

}
