// Play time of a Playnite game (app/backend/playtimetext), decision of 2026-11-26: under an hour
// « N min jouées », then « N h jouées », never played « Jamais lancé ». The library's column has
// the short form. The same values as the Android client's test.

#include "../../app/backend/playtimetext.h"

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
    expect(PlaytimeText::played(0), "Jamais lancé", "neverPlayed");
    expect(PlaytimeText::played(-5), "Jamais lancé", "negativeIsNeverPlayed");
    expect(PlaytimeText::played(20), "1 min jouées", "secondsShowOneMinute");
    expect(PlaytimeText::played(25 * 60), "25 min jouées", "twentyFiveMinutes");
    expect(PlaytimeText::played(25 * 60 + 59), "25 min jouées", "minutesRoundDown");
    expect(PlaytimeText::played(3599), "59 min jouées", "justUnderAnHour");
    expect(PlaytimeText::played(3600), "1 h jouées", "oneHour");
    expect(PlaytimeText::played(3600 + 29 * 60), "1 h jouées", "hoursRoundToNearestDown");
    expect(PlaytimeText::played(3600 + 30 * 60), "2 h jouées", "hoursRoundToNearestUp");
    expect(PlaytimeText::played(214 * 3600), "214 h jouées", "manyHours");

    expect(PlaytimeText::shortForm(0), "—", "shortNeverPlayed");
    expect(PlaytimeText::shortForm(25 * 60), "25 min", "shortMinutes");
    expect(PlaytimeText::shortForm(12 * 3600), "12 h", "shortHours");

    if (failures == 0) {
        std::printf("tst_playtime: all passed\n");
    }
    return failures == 0 ? 0 : 1;
}
