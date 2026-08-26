#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>

class QFile;
class QJsonObject;
class QNetworkReply;

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

signals:
    void onUpdateAvailable(QString newVersion, QString url);
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

    void parseStringToVersionQuad(QString& string, QVector<int>& version);

    int compareVersion(QVector<int>& version1, QVector<int>& version2);

    // Compares two version strings of the form x.y.z with an optional
    // "-ci.N" prerelease suffix. A stable release sorts after a prerelease
    // with the same base version.
    int compareVersionStrings(const QString& a, const QString& b);

    QString getPlatform();

    // Returns the browser_download_url of the release asset matching the
    // current platform and CPU architecture, or an empty string if none.
    QString findAssetDownloadUrl(const QJsonObject& releaseObj);

    QVector<int> m_CurrentVersionQuad;

    // Created per check and destroyed when it completes, so the bearer plugin
    // doesn't poll in the background between checks.
    QNetworkAccessManager* m_Nam;
    bool m_CheckInProgress;

    QPointer<QNetworkReply> m_DownloadReply;
    bool m_DownloadCanceled;
};
