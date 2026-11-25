#pragma once

#include "nvcomputer.h"
#include "nvhttp.h"
#include "settings/streamingpreferences.h"
#include "settings/compatfetcher.h"

#include <qmdnsengine/server.h>
#include <qmdnsengine/cache.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/resolver.h>

#include <QThread>
#include <QReadWriteLock>
#include <QSettings>
#include <QSet>
#include <QSslCertificate>
#include <QRunnable>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>

class ComputerManager;

class DelayedFlushThread : public QThread
{
    Q_OBJECT

public:
    DelayedFlushThread(ComputerManager* cm)
        : m_ComputerManager(cm)
    {
        setObjectName("CM Delayed Flush Thread");
    }

    void run();

private:
    ComputerManager* m_ComputerManager;
};

class MdnsPendingComputer : public QObject
{
    Q_OBJECT

public:
    explicit MdnsPendingComputer(const QSharedPointer<QMdnsEngine::Server> server,
                                 const QMdnsEngine::Service& service)
        : m_Hostname(service.hostname()),
          m_Port(service.port()),
          m_ServerWeak(server),
          m_Resolver(nullptr)
    {
        // Start resolving
        resolve();
    }

    virtual ~MdnsPendingComputer()
    {
        delete m_Resolver;
    }

    QString hostname()
    {
        return m_Hostname;
    }

    uint16_t port()
    {
        return m_Port;
    }

private slots:
    void handleResolvedTimeout()
    {
        if (m_Addresses.isEmpty()) {
            if (m_Retries-- > 0) {
                // Try again
                qInfo() << "Resolving" << hostname() << "timed out. Retrying...";
                resolve();
            }
            else {
                qWarning() << "Giving up on resolving" << hostname() << "after repeated failures";
                cleanup();
            }
        }
        else {
            Q_ASSERT(!m_Addresses.isEmpty());
            emit resolvedHost(this, m_Addresses);
        }
    }

    void handleResolvedAddress(const QHostAddress& address)
    {
        m_Addresses.push_back(address);
    }

signals:
    void resolvedHost(MdnsPendingComputer*,QVector<QHostAddress>&);

private:
    void cleanup()
    {
        // Delete our resolver, so we're guaranteed that nothing is referencing m_Server.
        delete m_Resolver;
        m_Resolver = nullptr;

        // Now delete our strong reference that we held on behalf of m_Resolver.
        // The server may be destroyed after we make this call.
        m_Server.reset();
    }

    void resolve()
    {
        // Clean up any existing resolver object and server references
        cleanup();

        // Re-acquire a strong reference if the server still exists.
        m_Server = m_ServerWeak.toStrongRef();
        if (!m_Server) {
            return;
        }

        m_Resolver = new QMdnsEngine::Resolver(m_Server.data(), m_Hostname);
        connect(m_Resolver, &QMdnsEngine::Resolver::resolved,
                this, &MdnsPendingComputer::handleResolvedAddress);
        QTimer::singleShot(2000, this, &MdnsPendingComputer::handleResolvedTimeout);
    }

    QByteArray m_Hostname;
    uint16_t m_Port;
    QWeakPointer<QMdnsEngine::Server> m_ServerWeak;
    QSharedPointer<QMdnsEngine::Server> m_Server;
    QMdnsEngine::Resolver* m_Resolver;
    QVector<QHostAddress> m_Addresses;
    int m_Retries = 10;
};

class ComputerPollingEntry
{
public:
    ComputerPollingEntry()
        : m_ActiveThread(nullptr)
    {

    }

    virtual ~ComputerPollingEntry()
    {
        interrupt();

        // interrupt() should have taken care of this
        Q_ASSERT(m_ActiveThread == nullptr);

        for (QThread* thread : std::as_const(m_InactiveList)) {
            thread->wait();
            delete thread;
        }
    }

    bool isActive()
    {
        cleanInactiveList();

        return m_ActiveThread != nullptr;
    }

    void setActiveThread(QThread* thread)
    {
        cleanInactiveList();

        Q_ASSERT(!isActive());
        m_ActiveThread = thread;
    }

    void interrupt()
    {
        cleanInactiveList();

        if (m_ActiveThread != nullptr) {
            // Interrupt the active thread
            m_ActiveThread->requestInterruption();

            // Place it on the inactive list awaiting death
            m_InactiveList.append(m_ActiveThread);

            m_ActiveThread = nullptr;
        }
    }

private:
    void cleanInactiveList()
    {
        QMutableListIterator<QThread*> i(m_InactiveList);

        // Reap any threads that have finished
        while (i.hasNext()) {
            i.next();

            QThread* thread = i.value();
            if (thread->isFinished()) {
                delete thread;
                i.remove();
            }
        }
    }

    QThread* m_ActiveThread;
    QList<QThread*> m_InactiveList;
};

class ComputerManager : public QObject
{
    Q_OBJECT

    friend class DeferredHostDeletionTask;
    friend class PendingAddTask;
    friend class PendingPairingTask;
    friend class PendingOTPPairingTask;
    friend class DelayedFlushThread;

public:
    explicit ComputerManager(StreamingPreferences* prefs);

    virtual ~ComputerManager();

    Q_INVOKABLE void startPolling();

    Q_INVOKABLE void stopPollingAsync();

    Q_INVOKABLE void addNewHostManually(QString address);

    // Test build: the message to show when a typed address is refused (loopback test hosts
    // only); empty when it is allowed, and always empty in any other build.
    Q_INVOKABLE QString testGuardRefusal(QString address);

    // userInput: what the user typed (addNewHostManually), given back by computerAddCompleted
    void addNewHost(NvAddress address, bool mdns, QString name = QString(), NvAddress mdnsIpv6Address = NvAddress(),
                    QString userInput = QString());

    QString generatePinString();

    // One pairing at a time (Android's HostsActivity.pairing): false, and nothing started, while
    // another one runs. The host's poller is stopped for the pairing and restarted after it.
    bool pairHost(NvComputer* computer, QString pin);

    bool pairHostWithOTP(NvComputer* computer, QString pin, QString passphrase);

    // The pairing under way: its host and the PIN shown (empty when none), so a page that opens
    // again shows the same dialog instead of starting a second pairing
    Q_INVOKABLE QString pairingUuid();
    Q_INVOKABLE QString pairingPin();

    // « Ajouter un PC » (Android's AddComputerManually): whether a typed address can be read at
    // all (address, address:port, [v6], [v6]:port, bare v6), and after a failed add, whether it
    // is a private IPv4 address outside every network of this PC (v2_add_wrong_subnet)
    Q_INVOKABLE static bool isValidManualAddress(QString address);
    Q_INVOKABLE static bool isWrongSubnetAddress(QString address);
    static bool parseManualAddress(QString address, NvAddress& result);

    // Pairing errors: Android's sentences (v2_pair_*), naming the host, translated
    enum class PairingError { Failed, WrongPin, InGame, Busy, UnknownHost, NotFound, Network, TimedOut, LinkExpired };
    static QString pairingErrorText(const QString& name, PairingError error, const QString& detail = QString());
    static QString pairingErrorText(const QString& name, const GfeHttpResponseException& e);
    static QString pairingErrorText(const QString& name, const QtNetworkReplyException& e);

    // devicename= of the pairing requests (percent-encoded): this PC's name, as in serverinfo
    static QString clientDeviceName();

    void quitRunningApp(NvComputer* computer);

    QVector<NvComputer*> getComputers();

    // The host leaves the list now (computerRemoved, then computerStateChanged, on this thread);
    // the object itself is deleted later, on this thread too, once its poller has stopped and no
    // pairing uses it. GUI thread only.
    void deleteHost(NvComputer* computer);

    void renameHost(NvComputer* computer, QString name);

    void clientSideAttributeUpdated(NvComputer* computer);

signals:
    void computerStateChanged(NvComputer* computer);

    // Emitted on the GUI thread by deleteHost(), before computerStateChanged(): whoever keeps
    // this pointer (AppModel) must drop it now and never read it again
    void computerRemoved(NvComputer* computer);

    // On the GUI thread, after the host's new state (paired) is in the list. error: empty on
    // success, otherwise a sentence for the user that names the host.
    void pairingCompleted(NvComputer* computer, QString error);

    // uuid: the host that answered at this address (new or already known), null on failure.
    // Always emitted after the computerStateChanged() that put a new host in the list, so a
    // ComputerModel already has its row when this arrives (HostsView pairs it from there).
    // address: what the user typed, so overlapping adds each find their own (HostsView)
    void computerAddCompleted(QVariant success, QVariant detectedPortBlocking, QVariant uuid, QVariant address);

    void quitAppCompleted(QVariant error);

private slots:
    void handleAboutToQuit();

    void handleComputerStateChanged(NvComputer* computer);

    void handleMdnsServiceResolved(MdnsPendingComputer* computer, QVector<QHostAddress>& addresses);

private:
    void saveHosts();

    void saveHost(NvComputer* computer);

    QHostAddress getBestGlobalAddressV6(QVector<QHostAddress>& addresses);

    void startPollingComputer(NvComputer* computer);

    // Pairing tasks end here, on the GUI thread: state, certificate, poller, signal
    void finishPairing(NvComputer* computer, QString error, QSslCertificate pairedCert);

    // One pairing at a time: false while another runs; takes the host's poller out
    bool beginPairing(NvComputer* computer, QString pin, ComputerPollingEntry*& pollingEntry);

    // Guarded: a removed host's late signals are dropped
    bool isKnownHost(NvComputer* computer);

    // Hosts and certificates another LavArtemis process stored (no single instance): merged by
    // the flush thread instead of being written over, adopted on the GUI thread
    QVector<NvComputer*> mergeStoredHosts();
    void adoptStoredHosts(QVector<NvComputer*> hosts);

    StreamingPreferences* m_Prefs;
    int m_PollingRef;
    QReadWriteLock m_Lock;
    QMap<QString, NvComputer*> m_KnownHosts;
    QMap<QString, ComputerPollingEntry*> m_PollEntries;
    QHash<QString, NvComputer> m_LastSerializedHosts; // Protected by m_DelayedFlushMutex
    QSharedPointer<QMdnsEngine::Server> m_MdnsServer;
    QMdnsEngine::Browser* m_MdnsBrowser;
    QVector<MdnsPendingComputer*> m_PendingResolution;
    CompatFetcher m_CompatFetcher;
    DelayedFlushThread* m_DelayedFlushThread;
    QMutex m_DelayedFlushMutex; // Lock ordering: Must never be acquired while holding NvComputer lock
    QWaitCondition m_DelayedFlushCondition;
    bool m_NeedsDelayedFlush;
    // Pairing under way (GUI thread; m_PairingUuid is also read under m_Lock by startPollingComputer)
    QString m_PairingUuid;
    QString m_PairingPin;
    NvComputer* m_PairingComputer = nullptr;
    // Forgotten during their pairing: deleted when it ends
    QVector<NvComputer*> m_DeleteAfterPairing;
    // Forgotten here (uuid): a flush never brings them back from what another process stored
    QSet<QString> m_ForgottenUuids;
};
