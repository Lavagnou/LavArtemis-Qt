#pragma once

#include <QObject>
#include <QNetworkAccessManager>

class QFile;
class QJsonObject;

class AutoUpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit AutoUpdateChecker(QObject *parent = nullptr);

    // Starts an asynchronous check against the GitHub releases of
    // Lavagnou/LavArtemis. Emits onUpdateAvailable if a newer version exists.
    Q_INVOKABLE void start();

    // Downloads the given release asset to the temp directory, reporting
    // progress through downloadProgress. Emits downloadReady on success.
    Q_INVOKABLE void downloadUpdate(QString url);

    // Launches a downloaded Windows installer detached and quits the app so
    // the WiX bundle can upgrade in place. Only meaningful on Windows.
    Q_INVOKABLE void installAndRestart(QString installerPath);

    // True when the current platform supports in-app download + install
    // (Windows x64/arm64). Other platforms should open the release page.
    Q_INVOKABLE bool canSelfInstall() const;

signals:
    void onUpdateAvailable(QString newVersion, QString url);
    void onUpdateDownloadUrl(QString downloadUrl);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadReady(QString filePath);
    void downloadFailed(QString error);

private slots:
    void handleUpdateCheckRequestFinished(QNetworkReply* reply);

private:
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
    QNetworkAccessManager* m_Nam;
};
