#pragma once

#include "computermanager.h"
#include <QDir>
#include <QImage>
#include <QThreadPool>
#include <QRunnable>
#include <QSet>

class BoxArtManager : public QObject
{
    Q_OBJECT

    friend class NetworkBoxArtLoadTask;

public:
    explicit BoxArtManager(QObject *parent = nullptr);

    QUrl
    loadBoxArt(NvComputer* computer, NvApp& app);

    // A Playnite game's background image (LavApollo: NvApp::playniteHasBackground), cached like the
    // covers. Empty while it loads (backgroundLoadComplete follows) and when the host has none.
    QUrl
    loadBackground(NvComputer* computer, NvApp& app);

    static
    void
    deleteBoxArt(const QString& computerUuid);

signals:
    // By uuid: the NvComputer may be gone when a fetch ends (a PC forgotten meanwhile)
    void
    boxArtLoadComplete(QString computerUuid, NvApp app, QUrl image);

    void
    backgroundLoadComplete(QString computerUuid, NvApp app, QUrl image);

public slots:

private slots:
    void
    handleBoxArtLoadComplete(QString computerUuid, NvApp app, QUrl image, bool background);

private:
    // Worker thread: copies only, never the NvComputer
    static
    QUrl
    loadBoxArtFromNetwork(const NvHttpTarget& target, const QString& cachePath, int appId, bool background);

    QString
    getFilePathForBoxArt(const QString& computerUuid, int appId, bool background = false);

    // Fetches under way, and covers the host does not have (LavApollo answers 404 for a Playnite
    // game without one): asked once per model, instead of on every data() call. Keyed by cache path.
    QSet<QString> m_Pending;
    QSet<QString> m_Missing;

    QDir m_BoxArtDir;
    QThreadPool m_ThreadPool;
};
