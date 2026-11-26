#include "librarycounttext.h"

#include <QCoreApplication>

namespace LibraryCountText {

QString line(const QString& hostName, int shown, int visible, bool allFilter)
{
    if (shown <= 0) {
        return QCoreApplication::translate("LibraryCountText", "%1 · aucun jeu").arg(hostName);
    }
    if (allFilter || shown == visible) {
        return shown == 1 ? QCoreApplication::translate("LibraryCountText", "%1 · 1 jeu").arg(hostName)
                          : QCoreApplication::translate("LibraryCountText", "%1 · %2 jeux").arg(hostName).arg(shown);
    }
    return shown == 1 ? QCoreApplication::translate("LibraryCountText", "%1 · 1 affiché sur %2").arg(hostName).arg(visible)
                      : QCoreApplication::translate("LibraryCountText", "%1 · %2 affichés sur %3")
                            .arg(hostName).arg(shown).arg(visible);
}

}
