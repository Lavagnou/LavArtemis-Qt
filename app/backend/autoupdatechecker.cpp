#include "autoupdatechecker.h"
#include "updaterules.h"
#include "settings/artemissettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#ifdef Q_OS_WIN32
#include <windows.h> // CREATE_NO_WINDOW
#endif

AutoUpdateChecker::AutoUpdateChecker(QObject *parent) :
    QObject(parent),
    m_Nam(nullptr),
    m_CheckInProgress(false),
    m_DownloadReply(nullptr),
    m_DownloadCanceled(false)
{
    qDebug() << "Current LavArtemis version:" << VERSION_STR;

    // An installer cannot delete itself: installAndRestart() hands it the file
    // and quits, so the only process that could clean up is the one being
    // replaced. The next start does it instead -- and by then the installer has
    // certainly finished, because the app it installed is the one running.
    cleanupDownloadedInstallers();
}

void AutoUpdateChecker::start()
{
    if (!ArtemisSettings::instance()->autoUpdateEnabled()) {
        qDebug() << "Update check disabled in settings";
        return;
    }

    m_AutomaticCheck = true;
    performCheck();
}

void AutoUpdateChecker::checkNow()
{
    // Deliberately ignores autoUpdateEnabled: that preference governs the check
    // at startup, not a button the user just pressed. Nor a skipped version: the user asked.
    m_AutomaticCheck = false;
    performCheck();
}

#define SKIPPED_VERSION_KEY "updateSkippedVersion"

void AutoUpdateChecker::skipVersion(QString version)
{
    QSettings().setValue(SKIPPED_VERSION_KEY, version);
}

void AutoUpdateChecker::performCheck()
{
#ifdef LAV_TEST_BUILD
    // Test build: never contacts GitHub. A check still ends with its one signal. LAV_TEST_UPDATE
    // (set by the bench) scripts the answers to the checks asked from Réglages, one per check,
    // separated by ';': "fail:<QNetworkReply::NetworkError>[:<HTTP status>]" or
    // "available:<version>[:<installer URL>]"; once they are used up, or for the startup check,
    // this version is the latest. They go through the same reporting as GitHub's answers.
    if (!m_AutomaticCheck) {
        static int s_TestCheck = 0;
        const QStringList outcomes = qEnvironmentVariable("LAV_TEST_UPDATE").split(';', Qt::SkipEmptyParts);
        const QString outcome = s_TestCheck < outcomes.size() ? outcomes[s_TestCheck] : QString();
        s_TestCheck++;
        if (outcome.startsWith(QLatin1String("fail:"))) {
            const QStringList parts = outcome.mid(5).split(':');
            emit updateCheckFailed(describeNetworkError(static_cast<QNetworkReply::NetworkError>(parts.value(0).toInt()),
                                                        parts.value(1).toInt()));
            return;
        }
        if (outcome.startsWith(QLatin1String("available:"))) {
            const QString rest = outcome.mid(10);
            const int colon = rest.indexOf(':');
            const QString version = colon < 0 ? rest : rest.left(colon);
            const QString installer = colon < 0 ? QString() : rest.mid(colon + 1);
            reportRelease(version, QStringLiteral("https://github.invalid/LavArtemis/releases/tag/v") + version, installer);
            return;
        }
    }
    emit noUpdateAvailable(QStringLiteral(VERSION_STR));
    return;
#endif
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

    // Point to LavArtemis GitHub releases. The channel is the build's own: the
    // stable app reads /releases/latest, which GitHub never points at a
    // prerelease; LavArtemis Dev (LAV_DEV_CHANNEL) needs the full list and keeps
    // the newest "-dev." tag (see pickDevRelease()). Dev releases are published
    // as prereleases, so neither app is ever offered the other's installer.
#ifdef LAV_DEV_CHANNEL
    QUrl url("https://api.github.com/repos/Lavagnou/LavArtemis/releases");
#else
    QUrl url("https://api.github.com/repos/Lavagnou/LavArtemis/releases/latest");
#endif
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", QByteArray("LavArtemis-Desktop/") + VERSION_STR);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#else
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, true);
#endif

    // An unanswered request (captive portal, stalled proxy) must end too: « Vérification… » would
    // turn for good, and m_CheckInProgress would refuse every later check
    request.setTransferTimeout(15000);

    m_CheckInProgress = true;
    m_Nam->get(request);
#else
    emit updateCheckFailed(tr("Updates are not handled in-app on this platform."));
#endif
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
    // Without data for 30 s, the download stops with a message instead of a bar that never moves
    request.setTransferTimeout(30000);

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
            qWarning() << "Update download failed:" << reply->error() << reply->errorString();
            emit downloadFailed(describeNetworkError(reply->error(),
                                                     reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
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

void AutoUpdateChecker::cleanupDownloadedInstallers()
{
#ifdef Q_OS_WIN32
    QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const QStringList leftovers = tempDir.entryList(
                QStringList() << QStringLiteral("LavArtemis-v*-win-installer.exe"),
                QDir::Files);
#ifdef LAV_DEV_CHANNEL
    const bool devChannel = true;
#else
    const bool devChannel = false;
#endif
    for (const QString& name : leftovers) {
        // Only this app's channel, and only what is not newer than this version: the stable app
        // and LavArtemis Dev share %TEMP%, and either may be downloading its own installer now
        if (!UpdateRules::isStaleInstaller(name, QStringLiteral(VERSION_STR), devChannel)) {
            continue;
        }

        QString path = tempDir.filePath(name);
        if (QFile::remove(path)) {
            qDebug() << "Removed stale update installer:" << path;
        }
        else {
            qWarning() << "Could not remove stale update installer:" << path;
        }
    }
#endif
}

void AutoUpdateChecker::installAndRestart(QString installerPath)
{
#ifdef Q_OS_WIN32
    // The WiX bundle upgrades in place. It runs /passive (progress bar only: no licence, no
    // "Suivant", no "Launch" to click; Windows still asks for the elevation), and a hidden cmd
    // waits for it, then starts the app again -- also when the install was cancelled, so the
    // user is never left without the app. cmd /c start /wait follows the bundle's own process,
    // which exits once its elevated engine is done.
    QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QProcess helper;
    helper.setProgram(QStringLiteral("cmd.exe"));
    helper.setNativeArguments(QStringLiteral("/c start \"\" /wait \"%1\" /passive /norestart & start \"\" \"%2\"")
                                  .arg(QDir::toNativeSeparators(installerPath), appPath));
    helper.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
    if (helper.startDetached()) {
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
            emit updateCheckFailed(tr("GitHub's answer could not be read."));
            return;
        }

        // /releases/latest returns a single object; /releases returns an array
        // ordered by creation date, most recent first.
        QJsonObject releaseObj;
        if (jsonDoc.isArray()) {
            QJsonArray releasesArray = jsonDoc.array();
            int picked = UpdateRules::pickDevRelease(releasesArray);
            if (picked < 0) {
                qWarning() << "GitHub API response doesn't contain any dev release";
                emit updateCheckFailed(tr("No release has been published yet."));
                return;
            }
            releaseObj = releasesArray[picked].toObject();
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
        reportRelease(version, releaseObj["html_url"].toString(),
                      UpdateRules::findAssetDownloadUrl(releaseObj, UpdateRules::platformAssetSuffix()));
    }
    else {
        qWarning() << "Update checking failed with error:" << reply->error() << reply->errorString();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError error = reply->error();
        reply->deleteLater();
        emit updateCheckFailed(describeNetworkError(error, httpStatus));
    }
}

void AutoUpdateChecker::reportRelease(const QString& version, const QString& pageUrl, const QString& downloadUrl)
{
    QString currentVersion(VERSION_STR);
    int res = UpdateRules::compareVersionStrings(currentVersion, version);
    if (res < 0) {
        // Current version < latest version
        if (m_AutomaticCheck && QSettings().value(SKIPPED_VERSION_KEY).toString() == version) {
            qDebug() << "Update available but skipped by the user:" << version;
            emit noUpdateAvailable(currentVersion);
            return;
        }
        qDebug() << "Update available";
        // Empty too: a release without an installer for this PC replaces the URL of an earlier one
        emit onUpdateDownloadUrl(downloadUrl);
        emit onUpdateAvailable(version, pageUrl);
    }
    else if (res > 0) {
        qDebug() << "GitHub release version lower than current version";
        emit noUpdateAvailable(currentVersion);
    }
    else {
        qDebug() << "GitHub release version equal to current version";
        emit noUpdateAvailable(currentVersion);
    }
}

QString AutoUpdateChecker::describeNetworkError(QNetworkReply::NetworkError error, int httpStatus)
{
    switch (error) {
    case QNetworkReply::OperationCanceledError: // what the transfer timeout aborts with
    case QNetworkReply::TimeoutError:
        return tr("GitHub did not answer in time. Try again later.");
    case QNetworkReply::SslHandshakeFailedError:
        return tr("The secure connection to GitHub failed. Check this PC's date and time.");
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
        return tr("GitHub cannot be reached. Check this PC's Internet connection.");
    default:
        break;
    }
    if (httpStatus > 0) {
        // 403: GitHub's limit on anonymous requests per hour, the likeliest refusal
        return tr("GitHub refused the request (HTTP error %1). Try again later.").arg(httpStatus);
    }
    return tr("Network error (%1).").arg(static_cast<int>(error));
}
