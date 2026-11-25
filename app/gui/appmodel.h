#pragma once

#include "backend/boxartmanager.h"
#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>

class AppModel : public QAbstractListModel
{
    Q_OBJECT

public:
    // Public: AppKindModel filters on KindRole.
    enum Roles
    {
        NameRole = Qt::UserRole,
        RunningRole,
        BoxArtRole,
        HiddenRole,
        AppIdRole,
        AppCollectorGameRole,
        PlayniteGameRole,
        PlayniteHostRole,
        // "desktop", "playnite" (the Playnite front end), "game" (Playnite library) or "app":
        // the sections of the redesigned home (gui/v2, AppKindModel)
        KindRole,
    };

public:
    explicit AppModel(QObject *parent = nullptr);

    // Must be called before any QAbstractListModel functions. The host by uuid, not by a row of
    // some ComputerModel (rows move when a host is added or forgotten); false if it is not known.
    Q_INVOKABLE bool initialize(ComputerManager* computerManager, QString hostUuid, bool showHiddenGames);

    // The host this model shows; empty once it has been forgotten (computerLost was emitted)
    Q_INVOKABLE QString hostUuid();

    // Everything below takes the app id, not a row: a menu or a dialog opened on an app keeps
    // its id while the rows move under it (an app list poll, a game moving up the library).
    // Works for a hidden app too (Reprendre on a hidden running game).
    Q_INVOKABLE Session* createSessionForApp(int appId);

    // Row of this app among the visible apps, -1 if it has none
    Q_INVOKABLE int rowOfAppId(int appId);


    // LavApollo Link: whether this app starts inside the host's running Playnite front end,
    // keeping the session (no "quit the running app" first).
    Q_INVOKABLE bool startsInsidePlaynite(int appId);

    // The game running inside the host's Playnite (LavApollo Link), empty if none
    Q_INVOKABLE QString getRunningPlayniteGameName();

    // Row of the running app among the visible apps, -1 if none is running or it is hidden
    Q_INVOKABLE int getRunningAppIndex();

    // Section of the running app (see KindRole), empty if none
    Q_INVOKABLE QString getRunningAppKind();

    // Section of the redesigned home an app belongs to (see KindRole)
    static QString kindOf(const NvApp& app);

    Q_INVOKABLE int getRunningAppId();

    Q_INVOKABLE QString getRunningAppName();

    Q_INVOKABLE void quitRunningApp();

    Q_INVOKABLE void setAppHidden(int appId, bool hidden);

    // The redesigned home: how many apps the user hid on this host, and whether a row is one
    Q_INVOKABLE int hiddenAppCount();
    Q_INVOKABLE bool isAppHidden(int appId);


    // Filename to offer when saving a .art shortcut for this app, with
    // anything a filesystem would reject stripped out.
    Q_INVOKABLE QString suggestedArtFileName(int appId);

    // Writes a .art launcher file: the format the Android client exports and
    // this one already opens. Returns an empty string on success, or a message
    // fit to show the user.
    Q_INVOKABLE QString exportArtFile(int appId, const QUrl& destination);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleBoxArtLoaded(QString computerUuid, NvApp app, QUrl image);

    void handleComputerRemoved(NvComputer* computer);

signals:
    void computerLost();

    // The app running on the host changed (getRunningAppId() and friends), also when neither
    // app has a row here (hidden, or not listed)
    void runningAppChanged();

private:
    void updateAppList(QVector<NvApp> newList);

    QVector<NvApp> getVisibleApps(const QVector<NvApp>& appList);

    bool isAppCurrentlyVisible(const NvApp& app);

    // The app with this id in the host's whole list (hidden ones too); false if none
    bool findApp(int appId, NvApp& app);

    // Null once the host is forgotten (ComputerManager::computerRemoved): never read it then
    NvComputer* m_Computer = nullptr;
    QString m_HostUuid;
    BoxArtManager m_BoxArtManager;
    ComputerManager* m_ComputerManager;
    QVector<NvApp> m_VisibleApps, m_AllApps;
    int m_CurrentGameId;
    bool m_ShowHiddenGames;
};
