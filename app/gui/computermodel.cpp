#include "computermodel.h"
#include "appmodel.h"
#include "backend/serverpermissions.h"
#include "settings/artemissettings.h"

#include <QThreadPool>
#include <QPointer>
#include <QCoreApplication>

ComputerModel::ComputerModel(QObject* object)
    : QAbstractListModel(object) {}

void ComputerModel::initialize(ComputerManager* computerManager)
{
    m_ComputerManager = computerManager;
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerModel::handleComputerStateChanged);
    connect(m_ComputerManager, &ComputerManager::pairingCompleted,
            this, &ComputerModel::handlePairingCompleted);
    connect(m_ComputerManager, &ComputerManager::computerRemoved,
            this, &ComputerModel::handleComputerRemoved);

    m_Computers = m_ComputerManager->getComputers();
}

QVariant ComputerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    Q_ASSERT(index.row() < m_Computers.count());

    NvComputer* computer = m_Computers[index.row()];
    QReadLocker lock(&computer->lock);

    switch (role) {
    case NameRole:
        return computer->name;
    case OnlineRole:
        return computer->state == NvComputer::CS_ONLINE;
    case PairedRole:
        return computer->pairState == NvComputer::PS_PAIRED;
    case BusyRole:
        return computer->currentGameId != 0;
    case WakeableRole:
        return !computer->macAddress.isEmpty();
    case StatusUnknownRole:
        return computer->state == NvComputer::CS_UNKNOWN;
    case ServerSupportedRole:
        return computer->isSupportedServerVersion;
    case ApolloVersionRole:
        return computer->apolloVersion;
    case UuidRole:
        return computer->uuid;
    case AddressRole:
        return computer->activeAddress.isNull() ? computer->manualAddress.toString()
                                                : computer->activeAddress.toString();
    case RunningAppRole: {
        if (computer->currentGameId == 0) {
            return QString();
        }
        if (!computer->playniteGameName.isEmpty()) {
            return computer->playniteGameName;
        }
        for (const NvApp& app : computer->appList) {
            if (app.id == computer->currentGameId) {
                // The names the home shows for its two cards
                QString kind = AppModel::kindOf(app);
                if (kind == QStringLiteral("desktop")) {
                    return tr("Bureau");
                }
                if (kind == QStringLiteral("playnite")) {
                    return QStringLiteral("Playnite Fullscreen");
                }
                return app.name;
            }
        }
        return QString();
    }
    case ServerVersionRole:
        return computer->appVersion;
    case ThisPcRole:
        return computer->isThisPc;
    default:
        return QVariant();
    }
}

int ComputerModel::rowCount(const QModelIndex& parent) const
{
    // We should not return a count for valid index values,
    // only the parent (which will not have a "valid" index).
    if (parent.isValid()) {
        return 0;
    }

    return m_Computers.count();
}

QHash<int, QByteArray> ComputerModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[NameRole] = "name";
    names[OnlineRole] = "online";
    names[PairedRole] = "paired";
    names[BusyRole] = "busy";
    names[WakeableRole] = "wakeable";
    names[StatusUnknownRole] = "statusUnknown";
    names[ServerSupportedRole] = "serverSupported";
    names[ApolloVersionRole] = "apolloVersion";
    names[UuidRole] = "uuid";
    names[AddressRole] = "address";
    names[RunningAppRole] = "runningApp";
    names[ServerVersionRole] = "serverVersion";
    names[ThisPcRole] = "thisPc";

    return names;
}

void ComputerModel::deleteComputer(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    // The row leaves through computerRemoved, in every ComputerModel at once (the home has its own)
    m_ComputerManager->deleteHost(m_Computers[computerIndex]);
}

void ComputerModel::handleComputerRemoved(NvComputer* computer)
{
    int index = m_Computers.indexOf(computer);
    if (index < 0) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_Computers.removeAt(index);
    endRemoveRows();
}

class DeferredWakeHostTask : public QRunnable
{
public:
    DeferredWakeHostTask(ComputerModel* model, NvComputer* computer)
        : m_Model(model), m_Computer(computer)
    {
        QReadLocker lock(&computer->lock);
        m_Uuid = computer->uuid;
        m_Name = computer->name;
    }

    void run()
    {
        bool sent = m_Computer->wake();
        QPointer<ComputerModel> model = m_Model;
        QString uuid = m_Uuid, name = m_Name;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [model, uuid, name, sent] {
            if (model) {
                emit model->wakeCompleted(uuid, name, sent);
            }
        }, Qt::QueuedConnection);
    }

private:
    QPointer<ComputerModel> m_Model;
    NvComputer* m_Computer;
    QString m_Uuid;
    QString m_Name;
};

void ComputerModel::wakeComputer(int computerIndex)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    DeferredWakeHostTask* wakeTask = new DeferredWakeHostTask(this, m_Computers[computerIndex]);
    QThreadPool::globalInstance()->start(wakeTask);
}

void ComputerModel::renameComputer(int computerIndex, QString name)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    m_ComputerManager->renameHost(m_Computers[computerIndex], name);
}

QString ComputerModel::generatePinString()
{
    return m_ComputerManager->generatePinString();
}

bool ComputerModel::pairComputer(int computerIndex, QString pin)
{
    Q_ASSERT(computerIndex < m_Computers.count());

    return m_ComputerManager->pairHost(m_Computers[computerIndex], pin);
}

void ComputerModel::handlePairingCompleted(NvComputer* computer, QString error)
{
    // Still alive here (a host forgotten during its pairing is deleted after this signal)
    QString uuid;
    {
        QReadLocker lock(&computer->lock);
        uuid = computer->uuid;
    }
    emit pairingCompleted(uuid, error.isEmpty() ? QVariant() : error);
}

void ComputerModel::handleComputerStateChanged(NvComputer* computer)
{
    QVector<NvComputer*> newComputerList = m_ComputerManager->getComputers();

    // Reset the model if the structural layout of the list has changed
    if (m_Computers != newComputerList) {
        beginResetModel();
        m_Computers = newComputerList;
        endResetModel();
    }
    else {
        // Let the view know that this specific computer changed
        int index = m_Computers.indexOf(computer);
        emit dataChanged(createIndex(index, 0), createIndex(index, 0));
    }
}

