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
                isPlayniteHost == other.isPlayniteHost;
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
};

Q_DECLARE_METATYPE(NvApp)
