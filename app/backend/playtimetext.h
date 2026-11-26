#pragma once

#include <QString>

// Play time of a Playnite game (PlaynitePlaytime, seconds), as the prototype writes it
// (decision of 2026-11-26): under an hour « 25 min jouées », then « 3 h jouées », never
// played « Jamais lancé ». Pure functions, tested by tests/playtime; shown through
// SystemProperties.playtimeText() / playtimeShort().
namespace PlaytimeText {

// « Jamais lancé », « 25 min jouées », « 3 h jouées »
QString played(qint64 seconds);

// The library's column: « — », « 25 min », « 3 h »
QString shortForm(qint64 seconds);

}
