#include "channelmigration.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace ChannelMigration
{

int copySettings(QSettings& from, QSettings& to)
{
    const QStringList keys = from.allKeys();
    for (const QString& key : keys) {
        to.setValue(key, from.value(key));
    }
    to.sync();
    return keys.size();
}

int copyTopLevelFiles(const QDir& from, const QDir& to)
{
    if (!from.exists()) {
        return 0;
    }
    if (!to.exists() && !QDir().mkpath(to.absolutePath())) {
        return 0;
    }

    int copied = 0;
    const QFileInfoList files = from.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& file : files) {
        QString target = to.absoluteFilePath(file.fileName());
        if (QFileInfo::exists(target)) {
            continue;
        }
        if (QFile::copy(file.absoluteFilePath(), target)) {
            copied++;
        }
    }
    return copied;
}

void importStableSettingsOnce()
{
#if defined(LAV_DEV_CHANNEL) && !defined(LAV_TEST_BUILD)
    // Not for the test build: it must start empty and never read another app's settings.
    static const char* kImportedKey = "lavDevImportedFromStable";

    QSettings dev;
    if (dev.value(kImportedKey, false).toBool() || !dev.allKeys().isEmpty()) {
        // Already imported, or this Dev install has a life of its own.
        return;
    }

    QSettings stable(QStringLiteral("LavArtemis"), QCoreApplication::applicationName());
    int keys = copySettings(stable, dev);

    // AppConfigLocation is <GenericConfigLocation>/<organisation>/<application>:
    // the stable app's one only differs by its organisation.
    QDir genericConfig(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
    QDir stableConfig(genericConfig.absoluteFilePath(QStringLiteral("LavArtemis/") + QCoreApplication::applicationName()));
    QDir devConfig(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    int files = copyTopLevelFiles(stableConfig, devConfig);

    dev.setValue(kImportedKey, true);
    dev.sync();

    qInfo() << "LavArtemis Dev: imported" << keys << "settings and" << files
            << "files from the stable app";
#endif
}

}
