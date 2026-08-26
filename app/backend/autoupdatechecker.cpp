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
    QObject(parent),
    m_Nam(nullptr),
    m_CheckInProgress(false),
    m_DownloadReply(nullptr),
    m_DownloadCanceled(false)
{
    QString currentVersion(VERSION_STR);
    qDebug() << "Current Artemis version:" << currentVersion;
    parseStringToVersionQuad(currentVersion, m_CurrentVersionQuad);

    // Should at least have a 1.0-style version number
    Q_ASSERT(m_CurrentVersionQuad.count() > 1);
}

void AutoUpdateChecker::start()
{
    if (!ArtemisSettings::instance()->autoUpdateEnabled()) {
        qDebug() << "Update check disabled in settings";
        return;
    }

    performCheck();
}

void AutoUpdateChecker::checkNow()
{
    // Deliberately ignores autoUpdateEnabled: that preference governs the check
    // at startup, not a button the user just pressed.
    performCheck();
}

void AutoUpdateChecker::performCheck()
{
#if defined(Q_OS_WIN32) || defined(Q_OS_DARWIN) || defined(STEAM_LINK) || defined(APP_IMAGE) // Only run update checker on platforms without auto-update
    if (m_CheckInProgress) {
        qDebug() << "Update check already in progress";
        return;
    }

    // A fresh manager per check. The previous one is destroyed once its reply
    // has been handled, so this is also what makes a second check possible at
    // all -- the old code kept the deleted manager's null pointer and every
    // later check returned without sending anything.
    m_Nam = new QNetworkAccessManager(this);

    // Never communicate over HTTP
    m_Nam->setStrictTransportSecurityEnabled(true);

    // Allow HTTP redirects
    m_Nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    connect(m_Nam, &QNetworkAccessManager::finished,
            this, &AutoUpdateChecker::handleUpdateCheckRequestFinished);

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

    m_CheckInProgress = true;
    m_Nam->get(request);
#else
    emit updateCheckFailed(tr("Updates are not handled in-app on this platform."));
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
    // One installer covers both architectures: the WiX bundle carries the x64
    // and the arm64 MSI and picks by NativeMachine. The per-arch zips published
    // beside it are portable builds with no installer inside, so pointing arm64
    // at one of those left installAndRestart() with a .zip to execute.
    QString wantedSuffix = QStringLiteral("-win-installer.exe");
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
    if (m_DownloadReply) {
        qDebug() << "Update download already in progress";
        return;
    }

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

    m_DownloadReply = reply;
    m_DownloadCanceled = false;

    // GitHub answers an asset URL with a redirect to its object store. Qt
    // follows it on the same reply, so whatever was written before the redirect
    // belongs to that response and not to the installer.
    connect(reply, &QNetworkReply::redirected,
            this, [file](const QUrl&) {
        file->seek(0);
        file->resize(0);
    });

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
        file->write(reply->readAll());
        file->flush();
        file->close();
        file->deleteLater();

        bool canceled = m_DownloadCanceled;
        m_DownloadReply = nullptr;
        m_DownloadCanceled = false;

        if (canceled) {
            QFile::remove(tempPath);
        }
        else if (reply->error() == QNetworkReply::NoError) {
            emit downloadReady(tempPath);
        }
        else {
            QFile::remove(tempPath);
            emit downloadFailed(reply->errorString());
        }

        reply->deleteLater();
        downloadNam->deleteLater();
    });
}

void AutoUpdateChecker::cancelDownload()
{
    if (!m_DownloadReply) {
        return;
    }

    // No downloadFailed follows an abort: the caller asked for it, and the
    // dialog would otherwise report its own Cancel button as an error.
    m_DownloadCanceled = true;
    m_DownloadReply->abort();
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

    // Delete the QNetworkAccessManager to free resources and prevent the bearer
    // plugin from polling in the background. performCheck() builds a new one.
    m_Nam->deleteLater();
    m_Nam = nullptr;
    m_CheckInProgress = false;

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
            emit updateCheckFailed(error.errorString());
            return;
        }

        // /releases/latest returns a single object; /releases returns an array
        // ordered by creation date, most recent first.
        QJsonObject releaseObj;
        if (jsonDoc.isArray()) {
            QJsonArray releasesArray = jsonDoc.array();
            if (releasesArray.isEmpty()) {
                qWarning() << "GitHub API response doesn't contain any releases";
                emit updateCheckFailed(tr("No release has been published yet."));
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
            emit updateCheckFailed(tr("The latest release carries no version tag."));
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
            emit noUpdateAvailable(currentVersion);
            return;
        }
        else {
            qDebug() << "GitHub release version equal to current version";
            emit noUpdateAvailable(currentVersion);
            return;
        }
    }
    else {
        qWarning() << "Update checking failed with error:" << reply->error();
        QString errorString = reply->errorString();
        reply->deleteLater();
        emit updateCheckFailed(errorString);
    }
}
