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

    static
    void
    deleteBoxArt(const QString& computerUuid);

signals:
    void
    boxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image);

public slots:

private slots:
    void
    handleBoxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image);

private:
    QUrl
    loadBoxArtFromNetwork(NvComputer* computer, int appId);

    QString
    getFilePathForBoxArt(NvComputer* computer, int appId);

    // Fetches under way, and covers the host does not have (LavApollo answers 404 for a Playnite
    // game without one): asked once per model, instead of on every data() call. Keyed by cache path.
    QSet<QString> m_Pending;
    QSet<QString> m_Missing;

    QDir m_BoxArtDir;
    QThreadPool m_ThreadPool;
};
