#pragma once

#include <QSettings>

class NvApp
{
public:
    NvApp() {}
    explicit NvApp(QSettings& settings);

    bool operator==(const NvApp& other) const
    {
        return id == other.id &&
                name == other.name &&
                uuid == other.uuid &&
                hdrSupported == other.hdrSupported &&
                isAppCollectorGame == other.isAppCollectorGame &&
                hidden == other.hidden &&
                directLaunch == other.directLaunch &&
                isPlayniteGame == other.isPlayniteGame &&
                isPlayniteHost == other.isPlayniteHost &&
                playniteInstalled == other.playniteInstalled &&
                playniteLastActivity == other.playniteLastActivity &&
                playnitePlaytime == other.playnitePlaytime &&
                playniteHasBackground == other.playniteHasBackground;
    }

    bool operator!=(const NvApp& other) const
    {
        return !operator==(other);
    }

    bool isInitialized()
    {
        // We use isNull() instead of isEmpty() here because we want
        // to detect cases where the name is unassigned, not empty.
        return id != 0 && !name.isNull();
    }

    void
    serialize(QSettings& settings) const;

    int id = 0;
    QString name;
    QString uuid;
    bool hdrSupported = false;
    bool isAppCollectorGame = false;
    bool hidden = false;
    bool directLaunch = false;
    // A game of the host's Playnite library (LavApollo Link): picked while the host's Playnite
    // front end runs, it starts inside Playnite without ending the session.
    bool isPlayniteGame = false;
    // The host's Playnite front end (LavApollo Link), where those games start.
    bool isPlayniteHost = false;
    // A Playnite game's state, from LavApollo's /applist (absent on older hosts: these defaults).
    // Installed: a game that is not is installed by Playnite when launched.
    bool playniteInstalled = true;
    // Last time it was played, Unix seconds (0: never, or an older host)
    qint64 playniteLastActivity = 0;
    // Play time, seconds
    qint64 playnitePlaytime = 0;
    // The host serves a background image for it (/appasset?asset=background)
    bool playniteHasBackground = false;
};

Q_DECLARE_METATYPE(NvApp)
