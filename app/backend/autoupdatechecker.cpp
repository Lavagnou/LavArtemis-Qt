#include "autoupdatechecker.h"
#include "settings/artemissettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

AutoUpdateChecker::AutoUpdateChecker(QObject *parent) :
    QObject(parent)
{
    m_Nam = new QNetworkAccessManager(this);

    // Never communicate over HTTP
    m_Nam->setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(m_Nam, &QNetworkAccessManager::finished,
            this, &AutoUpdateChecker::handleUpdateCheckRequestFinished);

    QString currentVersion(VERSION_STR);
    qDebug() << "Current Artemis version:" << currentVersion;
    parseStringToVersionQuad(currentVersion, m_CurrentVersionQuad);

    // Should at least have a 1.0-style version number
    Q_ASSERT(m_CurrentVersionQuad.count() > 1);
}

void AutoUpdateChecker::start()
{
    if (!m_Nam) {
        Q_ASSERT(m_Nam);
        return;
    }

    if (!ArtemisSettings::instance()->autoUpdateEnabled()) {
        qDebug() << "Update check disabled in settings";
        return;
    }

#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE) // Only run update checker on platforms without auto-update
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    // Point to LavArtemis GitHub releases. /releases/latest returns the most
    // recent stable release (CI builds are published as prereleases); the
    // prerelease channel needs the full list to also see CI builds.
    bool includePrerelease = ArtemisSettings::instance()->autoUpdatePrerelease();
    QUrl url(includePrerelease
             ? "https://api.github.com/repos/Lavagnou/LavArtemis/releases"
             : "https://api.github.com/repos/Lavagnou/LavArtemis/releases/latest");
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QByteArray("LavArtemis-Desktop/") + VERSION_STR);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#else
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
#endif
    m_Nam->get(request);
#endif
}

void AutoUpdateChecker::parseStringToVersionQuad(QString& string, QVector<int>& version)
{
    QStringList list = string.split('.');
    for (const QString& component : std::as_const(list)) {
        version.append(component.toInt());
    }
}

QString AutoUpdateChecker::getPlatform()
{
#if defined(STEAM_LINK)
    return QStringLiteral("steamlink");
#elif defined(APP_IMAGE)
    return QStringLiteral("appimage");
#elif defined(Q_OS_DARWIN) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 changed this from 'osx' to 'macos'. Use the old one
    // to be consistent (and not require another entry in the manifest).
    return QStringLiteral("osx");
#else
    return QSysInfo::productType();
#endif
}

int AutoUpdateChecker::compareVersion(QVector<int>& version1, QVector<int>& version2) {
    for (int i = 0;; i++) {
        int v1Val = 0;
        int v2Val = 0;

        // Treat missing decimal places as 0
        if (i < version1.count()) {
            v1Val = version1[i];
        }
        if (i < version2.count()) {
            v2Val = version2[i];
        }
        if (i >= version1.count() && i >= version2.count()) {
            // Equal versions
            return 0;
        }

        if (v1Val < v2Val) {
            return -1;
        }
        else if (v1Val > v2Val) {
            return 1;
        }
    }
}

int AutoUpdateChecker::compareVersionStrings(const QString& a, const QString& b)
{
    // Versions look like "20.4.0" or "20.4.0-ci.42"
    QStringList partsA = a.split('-');
    QStringList partsB = b.split('-');

    QString baseA = partsA.value(0);
    QString baseB = partsB.value(0);
    QVector<int> quadA, quadB;
    parseStringToVersionQuad(baseA, quadA);
    parseStringToVersionQuad(baseB, quadB);

    int baseCmp = compareVersion(quadA, quadB);
    if (baseCmp != 0) {
        return baseCmp;
    }

    // Same base: a stable release (no suffix) is newer than any prerelease
    bool preA = partsA.size() > 1;
    bool preB = partsB.size() > 1;
    if (preA != preB) {
        return preA ? -1 : 1;
    }
    if (preA) {
        // Both prereleases, e.g. "ci.42": compare the trailing build number
        int buildA = partsA.value(1).split('.').last().toInt();
        int buildB = partsB.value(1).split('.').last().toInt();
        return buildA - buildB;
    }
    return 0;
}

QString AutoUpdateChecker::findAssetDownloadUrl(const QJsonObject& releaseObj)
{
#ifdef Q_OS_WIN32
    // The combined WiX installer covers both architectures, but the published
    // artifacts are per-arch; pick the one matching the running binary.
    QString arch = QSysInfo::currentCpuArchitecture();
    QString wantedSuffix = (arch == "arm64")
            ? QStringLiteral("-win-arm64.zip")
            : QStringLiteral("-win-installer.exe");
#elif defined(Q_OS_DARWIN)
    QString wantedSuffix = QStringLiteral(".dmg");
#elif defined(APP_IMAGE)
    QString wantedSuffix = QStringLiteral("-linux-x86_64.AppImage");
#else
    return QString();
#endif

    QJsonArray assets = releaseObj["assets"].toArray();
    for (const QJsonValue& assetVal : assets) {
        QJsonObject asset = assetVal.toObject();
        if (asset["name"].toString().endsWith(wantedSuffix)) {
            return asset["browser_download_url"].toString();
        }
    }
    return QString();
}

bool AutoUpdateChecker::canSelfInstall() const
{
    // Only the Windows installer flow is automated. Linux AppImage has no
    // zsync published and macOS relies on the release page too.
#ifdef Q_OS_WIN32
    return true;
#else
    return false;
#endif
}

void AutoUpdateChecker::downloadUpdate(QString url)
{
    QNetworkAccessManager* downloadNam = new QNetworkAccessManager(this);
    downloadNam->setStrictTransportSecurityEnabled(true);
    downloadNam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkRequest request((QUrl(url)));
    request.setRawHeader("User-Agent", QByteArray("LavArtemis-Desktop/") + VERSION_STR);

    QNetworkReply* reply = downloadNam->get(request);

    QString fileName = QUrl(url).fileName();
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + "/" + fileName;

    QFile* file = new QFile(tempPath, reply);
    if (!file->open(QIODevice::WriteOnly)) {
        emit downloadFailed(tr("Cannot write to %1").arg(tempPath));
        reply->abort();
        reply->deleteLater();
        downloadNam->deleteLater();
        return;
    }

    connect(reply, &QNetworkReply::downloadProgress,
            this, [this](qint64 received, qint64 total) {
        emit downloadProgress(received, total);
    });

    connect(reply, &QNetworkReply::readyRead,
            this, [reply, file]() {
        file->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished,
            this, [this, reply, file, tempPath, downloadNam]() {
        file->flush();
        file->close();
        file->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            emit downloadReady(tempPath);
        } else {
            QFile::remove(tempPath);
            emit downloadFailed(reply->errorString());
        }

        reply->deleteLater();
        downloadNam->deleteLater();
    });
}

void AutoUpdateChecker::installAndRestart(QString installerPath)
{
#ifdef Q_OS_WIN32
    // The WiX bundle performs a MajorUpgrade in place, so we just hand it the
    // process and quit. startDetached keeps the installer alive after we exit.
    if (QProcess::startDetached(installerPath, QStringList())) {
        QCoreApplication::quit();
    } else {
        emit downloadFailed(tr("Failed to launch the installer"));
    }
#else
    Q_UNUSED(installerPath);
#endif
}

void AutoUpdateChecker::handleUpdateCheckRequestFinished(QNetworkReply* reply)
{
    Q_ASSERT(reply->isFinished());

    // Delete the QNetworkAccessManager to free resources and
    // prevent the bearer plugin from polling in the background.
    m_Nam->deleteLater();
    m_Nam = nullptr;

    if (reply->error() == QNetworkReply::NoError) {
        QTextStream stream(reply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif

        // Read all data and queue the reply for deletion
        QString jsonString = stream.readAll();
        reply->deleteLater();

        QJsonParseError error;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
        if (jsonDoc.isNull()) {
            qWarning() << "Update manifest malformed:" << error.errorString();
            return;
        }

        // /releases/latest returns a single object; /releases returns an array
        // ordered by creation date, most recent first.
        QJsonObject releaseObj;
        if (jsonDoc.isArray()) {
            QJsonArray releasesArray = jsonDoc.array();
            if (releasesArray.isEmpty()) {
                qWarning() << "GitHub API response doesn't contain any releases";
                return;
            }
            releaseObj = releasesArray[0].toObject();
        } else {
            releaseObj = jsonDoc.object();
        }

        // Extract version from tag_name (remove 'v' prefix if present)
        QString tagName = releaseObj["tag_name"].toString();
        QString version = tagName.startsWith("v") ? tagName.mid(1) : tagName;

        if (version.isEmpty()) {
            qWarning() << "GitHub release missing tag_name";
            return;
        }

        qDebug() << "Latest version of LavArtemis from GitHub:" << version;

        QString currentVersion(VERSION_STR);
        int res = compareVersionStrings(currentVersion, version);
        if (res < 0) {
            // Current version < latest version
            qDebug() << "Update available";
            QString downloadUrl = findAssetDownloadUrl(releaseObj);
            if (!downloadUrl.isEmpty()) {
                emit onUpdateDownloadUrl(downloadUrl);
            }
            emit onUpdateAvailable(version, releaseObj["html_url"].toString());
            return;
        }
        else if (res > 0) {
            qDebug() << "GitHub release version lower than current version";
            return;
        }
        else {
            qDebug() << "GitHub release version equal to current version";
            return;
        }
    }
    else {
        qWarning() << "Update checking failed with error:" << reply->error();
        reply->deleteLater();
    }
}
