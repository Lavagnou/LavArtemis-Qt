#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>

class ComputerModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        NameRole = Qt::UserRole,
        OnlineRole,
        PairedRole,
        BusyRole,
        WakeableRole,
        StatusUnknownRole,
        ServerSupportedRole,
        ApolloVersionRole,
        UuidRole,
        // Mes PC (gui/v2/HostsView): the address in use, and the name of what runs on the host
        AddressRole,
        RunningAppRole,
        // serverinfo's appversion, shown in « logiciel hôte non pris en charge (x.y.z.w) »
        ServerVersionRole,
        // The host is this PC (NvComputer::isThisPc): no stream to itself. Keep new roles last,
        // HostsView reads them by number.
        ThisPcRole
    };

public:
    explicit ComputerModel(QObject* object = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void deleteComputer(int computerIndex);

    Q_INVOKABLE QString generatePinString();

    Q_INVOKABLE void pairComputer(int computerIndex, QString pin);

    Q_INVOKABLE void wakeComputer(int computerIndex);

    Q_INVOKABLE void renameComputer(int computerIndex, QString name);


signals:
    void pairingCompleted(QVariant error);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handlePairingCompleted(NvComputer* computer, QString error);

private:
    QVector<NvComputer*> m_Computers;
    ComputerManager* m_ComputerManager;
};
