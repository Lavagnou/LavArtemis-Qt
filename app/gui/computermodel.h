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
        ThisPcRole,
        // LavApollo (prototype data): "tower", "mini", "laptop" or "" (the card's icon and « Tour »)
        FormFactorRole,
        // "wired", "wifi" or "": the host's link toward this client (« réseau filaire »)
        LinkRole,
        // Round trip to the host in ms (« En ligne · 4 ms »), -1 when unknown
        LatencyRole,
        // When the running app and the game running in Playnite started, ms since the epoch on
        // this PC's clock (« depuis 42 min »), 0 when unknown or nothing runs
        SessionStartedRole,
        PlayniteGameStartedRole,
        // Other devices streaming from this host now (« Rejoindre »), 0 when none or unknown
        OtherClientsRole
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

    // false when another pairing is under way (ComputerManager::pairHost): nothing started
    Q_INVOKABLE bool pairComputer(int computerIndex, QString pin);

    // Sends the wake-on-LAN packets off the GUI thread; wakeCompleted() says whether they left
    Q_INVOKABLE void wakeComputer(int computerIndex);

    Q_INVOKABLE void renameComputer(int computerIndex, QString name);


signals:
    // uuid of the host that was paired (or not); error undefined on success
    void pairingCompleted(QString uuid, QVariant error);

    void wakeCompleted(QString uuid, QString name, bool sent);

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleComputerRemoved(NvComputer* computer);

    void handlePairingCompleted(NvComputer* computer, QString error);

private:
    QVector<NvComputer*> m_Computers;
    ComputerManager* m_ComputerManager;
};
