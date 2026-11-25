#pragma once

#include <QObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>

class QFile;
class QJsonObject;

class AutoUpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit AutoUpdateChecker(QObject *parent = nullptr);

    // Runs the startup check against the GitHub releases of Lavagnou/LavArtemis.
    // Does nothing when the user turned startup checks off.
    Q_INVOKABLE void start();

    // The same check, requested explicitly from Settings. It runs even when
    // startup checks are disabled -- the user just asked for one.
    Q_INVOKABLE void checkNow();

    // « Ignorer cette version » (Android's UpdateChecker.skipVersion): the startup check stops
    // offering this version (it ends with noUpdateAvailable instead); checkNow() still offers it.
    Q_INVOKABLE void skipVersion(QString version);

    // Downloads the given release asset to the temp directory, reporting
    // progress through downloadProgress. Emits downloadReady on success.
    Q_INVOKABLE void downloadUpdate(QString url);

    // Aborts a download started by downloadUpdate(). No downloadFailed follows:
    // the caller already knows, it asked.
    Q_INVOKABLE void cancelDownload();

    // Launches a downloaded Windows installer detached and quits the app so
    // the WiX bundle can upgrade in place. Only meaningful on Windows.
    Q_INVOKABLE void installAndRestart(QString installerPath);

    // True when the current platform supports in-app download + install
    // (Windows x64/arm64). Other platforms should open the release page.
    Q_INVOKABLE bool canSelfInstall() const;

    // What a failed request means, in a sentence for the user (French through qml_fr.ts): the
    // QNetworkReply::errorString() of Qt is English and technical ("Host api.github.com not found").
    // httpStatus: the HTTP status of the answer, 0 when there was none.
    static QString describeNetworkError(QNetworkReply::NetworkError error, int httpStatus);

signals:
    void onUpdateAvailable(QString newVersion, QString url);
    // Emitted before every onUpdateAvailable, empty when the release has no installer for this
    // PC: the UI keeps the last one, and a stale URL would install an older release.
    void onUpdateDownloadUrl(QString downloadUrl);

    // Every check ends in exactly one of onUpdateAvailable, noUpdateAvailable
    // or updateCheckFailed. Without the last two, a UI that shows "Checking..."
    // has nothing to stop waiting on -- which is the common case, since most
    // checks find nothing.
    void noUpdateAvailable(QString currentVersion);
    void updateCheckFailed(QString error);

    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadReady(QString filePath);
    void downloadFailed(QString error);

private slots:
    void handleUpdateCheckRequestFinished(QNetworkReply* reply);

private:
    // Issues the request. Both start() and checkNow() land here once they have
    // decided whether the check should happen at all.
    void performCheck();

    // The newest release found: compared with this version, then announced (onUpdateDownloadUrl
    // + onUpdateAvailable) or not (noUpdateAvailable, skipped version included).
    void reportRelease(const QString& version, const QString& pageUrl, const QString& downloadUrl);

    // Deletes update installers of this app's channel left in the temp directory
    // (UpdateRules::isStaleInstaller). Called once per process, at construction,
    // before any download of this run can start.
    void cleanupDownloadedInstallers();

    QString getPlatform();

    // The version rules (compare, Dev release, asset, stale installer) are in
    // backend/updaterules: pure functions, tested by tests/updater.

    // Created per check and destroyed when it completes, so the bearer plugin
    // doesn't poll in the background between checks.
    QNetworkAccessManager* m_Nam;
    bool m_CheckInProgress;
    // The check in progress is the startup one (start()), which honours a skipped version
    bool m_AutomaticCheck = false;

    QPointer<QNetworkReply> m_DownloadReply;
    bool m_DownloadCanceled;
};
