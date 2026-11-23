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
        DirectLaunchRole,
        AppCollectorGameRole,
        PlayniteGameRole,
        PlayniteHostRole,
        // "desktop", "playnite" (the Playnite front end), "game" (Playnite library) or "app":
        // the sections of the redesigned home (gui/v2, AppKindModel)
        KindRole,
    };

public:
    explicit AppModel(QObject *parent = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager, int computerIndex, bool showHiddenGames);

    Q_INVOKABLE Session* createSessionForApp(int appIndex);

    Q_INVOKABLE int getDirectLaunchAppIndex();

    // LavApollo Link: whether this app starts inside the host's running Playnite front end,
    // keeping the session (no "quit the running app" first).
    Q_INVOKABLE bool startsInsidePlaynite(int appIndex);

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

    Q_INVOKABLE void setAppHidden(int appIndex, bool hidden);

    Q_INVOKABLE void setAppDirectLaunch(int appIndex, bool directLaunch);

    // Filename to offer when saving a .art shortcut for this app, with
    // anything a filesystem would reject stripped out.
    Q_INVOKABLE QString suggestedArtFileName(int appIndex);

    // Writes a .art launcher file: the format the Android client exports and
    // this one already opens. Returns an empty string on success, or a message
    // fit to show the user.
    Q_INVOKABLE QString exportArtFile(int appIndex, const QUrl& destination);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);

    void handleBoxArtLoaded(NvComputer* computer, NvApp app, QUrl image);

signals:
    void computerLost();

private:
    void updateAppList(QVector<NvApp> newList);

    QVector<NvApp> getVisibleApps(const QVector<NvApp>& appList);

    bool isAppCurrentlyVisible(const NvApp& app);

    NvComputer* m_Computer;
    BoxArtManager m_BoxArtManager;
    ComputerManager* m_ComputerManager;
    QVector<NvApp> m_VisibleApps, m_AllApps;
    int m_CurrentGameId;
    bool m_ShowHiddenGames;
};
