// The library's game count (app/backend/librarycounttext): hidden games never count, so a filter
// that shows every visible game says « N jeux », not « N affichés sur N ».

#include "../../app/backend/librarycounttext.h"

#include <cstdio>

namespace {
int failures = 0;

void expect(const QString& actual, const char* expected, const char* description)
{
    if (actual != QString::fromUtf8(expected)) {
        std::fprintf(stderr, "FAIL: %s: got \"%s\", expected \"%s\"\n", description,
                     actual.toUtf8().constData(), expected);
        ++failures;
    }
}
}

int main()
{
    const QString pc = QStringLiteral("ATLAS-PC");

    expect(LibraryCountText::line(pc, 0, 265, true), "ATLAS-PC · aucun jeu", "emptyAll");
    expect(LibraryCountText::line(pc, 0, 265, false), "ATLAS-PC · aucun jeu", "emptyFilter");

    expect(LibraryCountText::line(pc, 265, 265, true), "ATLAS-PC · 265 jeux", "allFilter");
    expect(LibraryCountText::line(pc, 1, 1, true), "ATLAS-PC · 1 jeu", "allFilterOneGame");
    // Récents lists every visible game; one more game is hidden: not « 265 affichés sur 266 »
    expect(LibraryCountText::line(pc, 265, 265, false), "ATLAS-PC · 265 jeux", "everyVisibleGameInAFilter");
    expect(LibraryCountText::line(pc, 1, 1, false), "ATLAS-PC · 1 jeu", "everyVisibleGameInAFilterOne");

    expect(LibraryCountText::line(pc, 12, 265, false), "ATLAS-PC · 12 affichés sur 265", "partOfTheGames");
    expect(LibraryCountText::line(pc, 1, 265, false), "ATLAS-PC · 1 affiché sur 265", "oneOfTheGames");

    if (failures == 0) {
        std::printf("tst_librarycount: all passed\n");
    }
    return failures == 0 ? 0 : 1;
}
