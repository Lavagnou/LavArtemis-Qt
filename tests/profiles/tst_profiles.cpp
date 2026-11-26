// Settings profiles (app/settings/profilemanager): a new profile is a full snapshot of what is in
// effect -- every key the streaming settings read, with its current value, defaults included --
// made with no profile active as well as under one (review of waves 1-4, #4). Language and the
// app-wide keys stay global. StreamingPreferences is played by the value()/setValue() calls below,
// the way its reload() and save() make them.

#include "profilemanager.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

// Whether profile `id` itself stores `key` (testRawValue: what is stored, no fall-through)
bool profileHas(ProfileManager* pm, const QString& id, const QString& key)
{
    const QVariantList profiles = pm->testRawValue(key).value("profiles").toList();
    for (const QVariant& p : profiles) {
        const QVariantMap m = p.toMap();
        if (m.value("id").toString() == id) {
            return m.value("stored").toBool();
        }
    }
    return false;
}

// What StreamingPreferences::reload() does at startup: every key read once, with its default
void reload(ProfileManager* pm)
{
    pm->value("width", 1280);
    pm->value("height", 720);
    pm->value("fps", 60);
    pm->value("language", 0);
    pm->value("mdns", true);
}

void testSnapshotWithoutActiveProfile()
{
    ProfileManager* pm = ProfileManager::instance();
    reload(pm);
    // Global settings, no profile: the width was changed, the height never was (its default)
    pm->setValue("width", 2560);

    const QString id = pm->createProfile("Salon");

    // The globals change afterwards (no profile active): the new profile must not follow them
    pm->setValue("width", 1920);
    pm->setValue("height", 1080);
    pm->setValue("language", 2);

    pm->setActiveProfile(id);
    check(pm->value("width", 1280).toInt() == 2560, "the width in effect when the profile was made");
    check(pm->value("height", 720).toInt() == 720, "the height in effect then: its default, held by the profile");
    check(pm->value("fps", 60).toInt() == 60, "every key read is in the snapshot");
    check(pm->value("language", 0).toInt() == 2, "the language stays global");
    check(profileHas(pm, id, "width") && profileHas(pm, id, "height"), "held by the profile itself");
    check(!profileHas(pm, id, "language") && !profileHas(pm, id, "mdns"), "global keys are never copied");
    pm->setActiveProfile(QString());
}

void testSnapshotUnderAProfile()
{
    ProfileManager* pm = ProfileManager::instance();
    // A profile that holds only the fps (an older, sparse one): the rest falls through to the globals
    const QString sparse = pm->createProfile("Ancien");
    pm->setActiveProfile(sparse);
    pm->setValue("fps", 120);

    const QString id = pm->createProfile("Copie");
    pm->setActiveProfile(QString());
    pm->setValue("width", 3840);  // the globals change: the copy must not follow

    pm->setActiveProfile(id);
    check(pm->value("fps", 60).toInt() == 120, "under a profile: its values");
    check(pm->value("width", 1280).toInt() == 1920, "and the globals in effect then, frozen");
    pm->setActiveProfile(QString());
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    // Nothing written to the user's settings: an INI file in a temporary folder, and test paths
    QTemporaryDir dir;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QCoreApplication::setOrganizationName("LavArtemis Unit Tests");
    QCoreApplication::setApplicationName("tst_profiles");

    testSnapshotWithoutActiveProfile();
    testSnapshotUnderAProfile();
    if (failures == 0) {
        std::printf("tst_profiles: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}
