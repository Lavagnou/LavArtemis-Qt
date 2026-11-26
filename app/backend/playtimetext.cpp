#include "playtimetext.h"

#include <QCoreApplication>

namespace {

// Under an hour: whole minutes, at least 1 (a game played a few seconds was launched).
// From an hour: hours rounded to the nearest (1 h 29 → 1 h, 1 h 30 → 2 h).
bool underAnHour(qint64 seconds) { return seconds < 3600; }
qint64 minutes(qint64 seconds) { return qMax<qint64>(1, seconds / 60); }
qint64 hours(qint64 seconds) { return (seconds + 1800) / 3600; }

}

namespace PlaytimeText {

QString played(qint64 seconds)
{
    if (seconds <= 0) {
        return QCoreApplication::translate("PlaytimeText", "Jamais lancé");
    }
    if (underAnHour(seconds)) {
        return QCoreApplication::translate("PlaytimeText", "%1 min jouées").arg(minutes(seconds));
    }
    return QCoreApplication::translate("PlaytimeText", "%1 h jouées").arg(hours(seconds));
}

QString shortForm(qint64 seconds)
{
    if (seconds <= 0) {
        return QStringLiteral("—");
    }
    if (underAnHour(seconds)) {
        return QCoreApplication::translate("PlaytimeText", "%1 min").arg(minutes(seconds));
    }
    return QCoreApplication::translate("PlaytimeText", "%1 h").arg(hours(seconds));
}

}
