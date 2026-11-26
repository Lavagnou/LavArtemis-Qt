#pragma once

#include "nvhttp.h"
#include "nvaddress.h"

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QRunnable>

class CopySafeReadWriteLock : public QReadWriteLock
{
public:
    CopySafeReadWriteLock() = default;

    // Don't actually copy the QReadWriteLock
    CopySafeReadWriteLock(const CopySafeReadWriteLock&) : QReadWriteLock() {}
    CopySafeReadWriteLock& operator=(const CopySafeReadWriteLock &) { return *this; }
};

class NvComputer
{
    friend class PcMonitorThread;
    friend class ComputerManager;

private:
    void sortAppList();

    static void sortApps(QVector<NvApp>& apps);

    // Android's rule (HostSupport.isSupportedVersion): serverinfo's appversion must be a version
    // quad whose generation (first number) is at least 3, the oldest moonlight-common-c speaks.
    // Empty (not known yet) counts as supported. Part of isSupportedServerVersion.
    static bool isSupportedAppVersion(const QString& appVersion);

    // True when the host is this very PC: its address is a loopback or local one, or the MAC it
    // reports is one of this machine's adapters. Streaming a PC to itself is refused (isThisPc).
    static bool isThisMachine(const QByteArray& mac, const QString& address);

    bool updateAppList(QVector<NvApp> newAppList);

    bool pendingQuit;

public:
    NvComputer() = default;

    // Caller is responsible for synchronizing read access to the other host
    NvComputer(const NvComputer&) = default;

    // Caller is responsible for synchronizing read access to the other host
    NvComputer& operator=(const NvComputer &) = default;

    explicit NvComputer(NvHTTP& http, QString serverInfo);

    explicit NvComputer(QSettings& settings);

    void
    setRemoteAddress(QHostAddress);

    bool
    update(const NvComputer& that);

    bool
    wake() const;

    // A Playnite game touched while the Playnite front end runs on the host (another app is
    // current): the host starts it inside Playnite and the client joins the session (resume),
    // instead of quitting Playnite first. The home and the command line decide alike.
    // Takes the lock.
    bool
    startsInsidePlaynite(const NvApp& app);

    enum ReachabilityType
    {
        RI_UNKNOWN,
        RI_LAN,
        RI_VPN,
    };

    ReachabilityType
    getActiveAddressReachability() const;

    QVector<NvAddress>
    uniqueAddresses() const;

    void
    serialize(QSettings& settings, bool serializeApps) const;

    // Caller is responsible for synchronizing read access to both hosts
    bool
    isEqualSerialized(const NvComputer& that) const;

    enum PairState
    {
        PS_UNKNOWN,
        PS_PAIRED,
        PS_NOT_PAIRED
    };

    enum ComputerState
    {
        CS_UNKNOWN,
        CS_ONLINE,
        CS_OFFLINE
    };

    // Ephemeral traits
    ComputerState state;
    PairState pairState;
    NvAddress activeAddress;
    uint16_t activeHttpsPort;
    int currentGameId;
    QString gfeVersion;
    QString appVersion;
    QVector<NvDisplayMode> displayModes;
    int maxLumaPixelsHEVC;
    int serverCodecModeSupport;
    // Optional paired-host /serverinfo extension (LavApollo, Vibepollo): the
    // host's outbound wired link speed for PyroWave; zero means unknown.
    uint32_t pyrowaveHostLinkMbps = 0;
    QString gpuModel;
    bool isSupportedServerVersion;
    // The host runs on this PC (isThisMachine): launching a stream there is refused with a message
    bool isThisPc = false;
#ifdef LAV_TEST_BUILD
    // Test build: serverinfo says LavTestHost; every test host is on loopback, so "this PC"
    // comes from LavTestHostThisPc instead of isThisMachine()
    bool isTestHost = false;
    bool testHostThisPc = false;
#endif

    // Persisted traits
    NvAddress localAddress;
    NvAddress remoteAddress;
    NvAddress ipv6Address;
    NvAddress manualAddress;
    QByteArray macAddress;
    QString name;
    bool hasCustomName;
    QString uuid;
    QSslCertificate serverCert;
    QVector<NvApp> appList;
    bool isNvidiaServerSoftware;

    // Whether the host can emulate a whole monitor layout rather than a single display.
    // A host that predates the feature simply ignores the request and makes one display
    // the size of the layout, so this only decides whether the user is warned.
    bool isMultiDisplayCapable;

    // LavApollo's verdict on this client's microphone: "ready", "disabled", "no_permission",
    // "no_device", or empty for a host without microphone passthrough.
    QString micPassthroughState;
    // LavApollo's mode of the display a stream captures (0 = not announced): a client joining
    // or launching without a virtual display streams at it (HostDisplayFit)
    int currentDisplayWidth;
    int currentDisplayHeight;
    int currentDisplayRefreshRate;
    // LavApollo Link (Playnite): whether the running app is the host's Playnite front end, where a
    // Playnite game starts without ending the session, and the game running in Playnite (empty if none)
    bool playniteHostRunning;
    QString playniteGameName;
    // When the running app and the game running in Playnite started, in ms since the epoch on this
    // PC's clock (LavApollo sends how long ago: CurrentGameElapsed, PlayniteGameElapsed). 0 = unknown.
    qint64 currentGameStartedAt = 0;
    qint64 playniteGameStartedAt = 0;
    // What kind of PC the host is ("tower", "mini", "laptop") and whether its interface toward this
    // client is "wired" or "wifi" (LavApollo HostFormFactor / HostLink). Empty = unknown.
    QString hostFormFactor;
    QString hostLink;
    // How many other devices stream from the host right now (LavApollo OtherClientsConnected; 0 when
    // unknown): the home says « Rejoindre » instead of « Reprendre » when it is not 0
    int otherClientsConnected = 0;
    // Round trip to the host, ms: the TCP connect time of the latest poll (PcMonitorThread), at least
    // 1 when it answered. -1 = unknown (offline, never polled).
    int latencyMs = -1;

    // LavApollo's hidden "Benchmark" app (docs/benchmark.md), 0 / empty when the host
    // does not offer it. Not in appList: it is launched by these.
    int benchmarkAppId;
    QString benchmarkAppUuid;
    QStringList serverCommands;
    quint32 serverPermissions;
    // Remember to update isEqualSerialized() when adding fields here!
    QString apolloVersion;

    // Synchronization
    mutable CopySafeReadWriteLock lock;

private:
    uint16_t externalPort;
};
