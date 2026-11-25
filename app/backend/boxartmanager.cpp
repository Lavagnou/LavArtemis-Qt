#include "boxartmanager.h"
#include "../path.h"

#include <QImageReader>
#include <QImageWriter>

BoxArtManager::BoxArtManager(QObject *parent) :
    QObject(parent),
    m_BoxArtDir(Path::getBoxArtCacheDir()),
    m_ThreadPool(this)
{
    // 4 is a good balance between fast loading for large
    // app grids and not crushing GFE with tons of requests
    // and causing UI jank from constantly stalling to decode
    // new images.
    m_ThreadPool.setMaxThreadCount(4);
    if (!m_BoxArtDir.exists()) {
        m_BoxArtDir.mkpath(".");
    }
}

QString
BoxArtManager::getFilePathForBoxArt(const QString& computerUuid, int appId)
{
    QDir dir = m_BoxArtDir;

    // Create the cache directory if it did not already exist
    if (!dir.exists(computerUuid)) {
        dir.mkdir(computerUuid);
    }

    // Change to this computer's box art cache folder
    dir.cd(computerUuid);

    // Try to open the cached file
    return dir.filePath(QString::number(appId) + ".png");
}

// Runs on the pool with copies only (the host's uuid, how to reach it, the cache file): the GUI
// thread may delete the NvComputer meanwhile (a PC forgotten while its covers load).
class NetworkBoxArtLoadTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    NetworkBoxArtLoadTask(BoxArtManager* boxArtManager, const QString& computerUuid, const NvHttpTarget& target,
                          const QString& cachePath, NvApp& app)
        : m_ComputerUuid(computerUuid),
          m_Target(target),
          m_CachePath(cachePath),
          m_App(app)
    {
        connect(this, &NetworkBoxArtLoadTask::boxArtFetchCompleted,
                boxArtManager, &BoxArtManager::handleBoxArtLoadComplete);
    }

signals:
    void boxArtFetchCompleted(QString computerUuid, NvApp app, QUrl image);

private:
    void run()
    {
        QUrl image = BoxArtManager::loadBoxArtFromNetwork(m_Target, m_CachePath, m_App.id);
        if (image.isEmpty()) {
            // Give it another shot if it fails once
            image = BoxArtManager::loadBoxArtFromNetwork(m_Target, m_CachePath, m_App.id);
        }
        emit boxArtFetchCompleted(m_ComputerUuid, m_App, image);
    }

    QString m_ComputerUuid;
    NvHttpTarget m_Target;
    QString m_CachePath;
    NvApp m_App;
};

QUrl BoxArtManager::loadBoxArt(NvComputer* computer, NvApp& app)
{
    QString uuid;
    {
        QReadLocker lock(&computer->lock);
        uuid = computer->uuid;
    }

    // Try to open the cached file if it exists and contains data
    QFile cacheFile(getFilePathForBoxArt(uuid, app.id));
    if (cacheFile.exists() && cacheFile.size() > 0) {
        return QUrl::fromLocalFile(cacheFile.fileName());
    }

    // The placeholder: the redesigned interface shows the app's name instead (gui/v2/CoverTile)
    QString key = cacheFile.fileName();
    if (m_Pending.contains(key) || m_Missing.contains(key)) {
        return QUrl("qrc:/res/no_app_image.png");
    }
    m_Pending.insert(key);

    // If we get here, we need to fetch asynchronously.
    // Kick off a worker on our thread pool to do just that.
    NetworkBoxArtLoadTask* netLoadTask = new NetworkBoxArtLoadTask(this, uuid, NvHttpTarget::of(computer),
                                                                   cacheFile.fileName(), app);
    m_ThreadPool.start(netLoadTask);

    // Return the placeholder then we can notify the caller
    // later when the real image is ready.
    return QUrl("qrc:/res/no_app_image.png");
}

// By uuid: the host may already be deleted (ComputerManager::deleteHost, a host forgotten during
// its pairing is deleted on the GUI thread while this runs on a worker)
void BoxArtManager::deleteBoxArt(const QString& computerUuid)
{
    QDir dir(Path::getBoxArtCacheDir());

    // Delete everything in this computer's box art directory
    if (!computerUuid.isEmpty() && dir.cd(computerUuid)) {
        dir.removeRecursively();
    }
}

void BoxArtManager::handleBoxArtLoadComplete(QString computerUuid, NvApp app, QUrl image)
{
    QString key = getFilePathForBoxArt(computerUuid, app.id);
    m_Pending.remove(key);
    if (image.isEmpty()) {
        m_Missing.insert(key);
    }
    else {
        emit boxArtLoadComplete(computerUuid, app, image);
    }
}

QUrl BoxArtManager::loadBoxArtFromNetwork(const NvHttpTarget& target, const QString& cachePath, int appId)
{
    NvHTTP http(target);

    QImage image;
    try {
        image = http.getBoxArt(appId);
    } catch (...) {}

    // Cache the box art on disk if it loaded
    if (!image.isNull()) {
        if (image.save(cachePath)) {
            return QUrl::fromLocalFile(cachePath);
        }
        else {
            // A failed save() may leave a zero byte file. Make sure that's removed.
            QFile(cachePath).remove();
        }
    }

    return QUrl();
}

#include "boxartmanager.moc"
