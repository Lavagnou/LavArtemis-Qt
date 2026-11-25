#include "computermanager.h"
#include "boxartmanager.h"
#include "nvhttp.h"
#include "nvpairingmanager.h"
#include "otppairingmanager.h"
#include "identitymanager.h"

#include <Limelight.h>
#include <QtEndian>

#include <QThread>
#include <QThreadPool>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSysInfo>
#include <QNetworkInterface>
#include <QUrl>

#include <memory>
#include <random>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

#ifdef LAV_TEST_BUILD
#include "testguard.h"
#endif

#define SER_HOSTS "hosts"
#define SER_HOSTS_BACKUP "hostsbackup"

class PcMonitorThread : public QThread
{
    Q_OBJECT

#define TRIES_BEFORE_OFFLINING 2
#define POLLS_PER_APPLIST_FETCH 10

public:
    PcMonitorThread(NvComputer* computer)
        : m_Computer(computer)
    {
        setObjectName("Polling thread for " + computer->name);
    }

private:
    bool tryPollComputer(QNetworkAccessManager* nam, NvAddress address, bool& changed)
    {
        QSslCertificate serverCert;
        bool trueUid;
        {
            // A pairing (finishPairing) or a certificate adopted from another process writes it
            QReadLocker lock(&m_Computer->lock);
            serverCert = m_Computer->serverCert;
            trueUid = !m_Computer->isNvidiaServerSoftware;
        }
        NvHTTP http(address, 0, serverCert, trueUid, nam);

        QString serverInfo;
        try {
            serverInfo = http.getServerInfo(NvHTTP::NvLogLevel::NVLL_NONE, true);
        } catch (...) {
            return false;
        }

        NvComputer newState(http, serverInfo);

        // Ensure the machine that responded is the one we intended to contact
        if (m_Computer->uuid != newState.uuid) {
            qInfo() << "Found unexpected PC" << newState.name << "looking for" << m_Computer->name;
            return false;
        }

        changed = m_Computer->update(newState);
        return true;
    }

    bool updateAppList(QNetworkAccessManager* nam, bool& changed)
    {
        NvHTTP http(m_Computer, nam);

        QVector<NvApp> appList;

        try {
            appList = http.getAppList();
            if (appList.isEmpty()) {
                return false;
            }
        } catch (...) {
            return false;
        }

        QWriteLocker lock(&m_Computer->lock);
        changed = m_Computer->updateAppList(appList);
        return true;
    }

    void run() override
    {
        // Reduce the power and performance impact of our
        // computer status polling while it's running.
        setPriority(QThread::LowPriority);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        setServiceLevel(QThread::QualityOfService::Eco);
#endif

        // Share the QNetworkAccessManager to conserve resources when polling.
        // Each instance creates a worker thread, so sharing them ensures that
        // we are not spamming a new thread for every single polling attempt.
        //
        // Since QThread inherit the priority of the current thread, this also
        // ensures that the NAM's worker thread will inherit our lower priority.
        QNetworkAccessManager nam;

        // Always fetch the applist the first time
        int pollsSinceLastAppListFetch = POLLS_PER_APPLIST_FETCH;
        while (!isInterruptionRequested()) {
            bool stateChanged = false;
            bool online = false;
            bool wasOnline = m_Computer->state == NvComputer::CS_ONLINE;
            for (int i = 0; i < (wasOnline ? TRIES_BEFORE_OFFLINING : 1) && !online; i++) {
                for (auto& address : m_Computer->uniqueAddresses()) {
                    if (isInterruptionRequested()) {
                        return;
                    }

                    if (tryPollComputer(&nam, address, stateChanged)) {
                        if (!wasOnline) {
                            qInfo() << m_Computer->name << "is now online at" << m_Computer->activeAddress.toString();
                        }
                        online = true;
                        break;
                    }
                }
            }

            // Check if we failed after all retry attempts
            // Note: we don't need to acquire the read lock here,
            // because we're on the writing thread.
            if (!online && m_Computer->state != NvComputer::CS_OFFLINE) {
                qInfo() << m_Computer->name << "is now offline";
                m_Computer->state = NvComputer::CS_OFFLINE;
                stateChanged = true;
            }

            // Grab the applist if it's empty or it's been long enough that we need to refresh
            pollsSinceLastAppListFetch++;
            if (m_Computer->state == NvComputer::CS_ONLINE &&
                    m_Computer->pairState == NvComputer::PS_PAIRED &&
                    (m_Computer->appList.isEmpty() || pollsSinceLastAppListFetch >= POLLS_PER_APPLIST_FETCH)) {
                // Notify prior to the app list poll since it may take a while, and we don't
                // want to delay onlining of a machine, especially if we already have a cached list.
                if (stateChanged) {
                    emit computerStateChanged(m_Computer);
                    stateChanged = false;
                }

                if (updateAppList(&nam, stateChanged)) {
                    pollsSinceLastAppListFetch = 0;
                }
            }

            if (stateChanged) {
                // Tell anyone listening that we've changed state
                emit computerStateChanged(m_Computer);
            }

            // Wait a bit to poll again, but do it in 100 ms chunks
            // so we can be interrupted reasonably quickly.
            // FIXME: QWaitCondition would be better.
            for (int i = 0; i < 30 && !isInterruptionRequested(); i++) {
                QThread::msleep(100);
            }
        }
    }

signals:
   void computerStateChanged(NvComputer* computer);

private:
    NvComputer* m_Computer;
};

ComputerManager::ComputerManager(StreamingPreferences* prefs)
    : m_Prefs(prefs),
      m_PollingRef(0),
      m_MdnsBrowser(nullptr),
      m_CompatFetcher(nullptr),
      m_NeedsDelayedFlush(false)
{
    QSettings settings;

    // If there's a hosts backup copy, we must have failed to commit
    // a previous update before exiting. Restore the backup now.
    int hosts = settings.beginReadArray(SER_HOSTS_BACKUP);
    if (hosts == 0) {
        // If there's no host backup, read from the primary location.
        settings.endArray();
        hosts = settings.beginReadArray(SER_HOSTS);
    }

    // Inflate our hosts from QSettings
    for (int i = 0; i < hosts; i++) {
        settings.setArrayIndex(i);
        NvComputer* computer = new NvComputer(settings);
        m_KnownHosts[computer->uuid] = computer;
        m_LastSerializedHosts[computer->uuid] = *computer;
    }
    settings.endArray();

    // Fetch latest compatibility data asynchronously
#ifndef LAV_TEST_BUILD // test build: no download from moonlight-stream.org (bundled/cached data only)
    m_CompatFetcher.start();
#endif

    // Start the delayed flush thread to handle saveHosts() calls
    m_DelayedFlushThread = new DelayedFlushThread(this);
    m_DelayedFlushThread->start();

    // To quit in a timely manner, we must block additional requests
    // after we receive the aboutToQuit() signal. This is necessary
    // because NvHTTP uses aboutToQuit() to abort requests in progress
    // while quitting, however this is a one time signal - additional
    // requests would not be aborted and block termination.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &ComputerManager::handleAboutToQuit);
}

ComputerManager::~ComputerManager()
{
    // Stop the delayed flush thread before acquiring the lock in write mode
    // to avoid deadlocking with a flush that needs the lock in read mode.
    {
        // Wake the delayed flush thread
        m_DelayedFlushThread->requestInterruption();
        m_DelayedFlushCondition.wakeOne();

        // Wait for it to terminate (and finish any pending flush)
        m_DelayedFlushThread->wait();
        delete m_DelayedFlushThread;

        // Delayed flushes should have completed by now
        Q_ASSERT(!m_NeedsDelayedFlush);
    }

    QWriteLocker lock(&m_Lock);

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        delete computer;
        m_PendingResolution.removeFirst();
    }

    // Delete the browser to stop discovery
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;

    // Interrupt polling
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }

    // Delete all polling entries (and associated threads)
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        delete entry;
    }

    // Destroy all NvComputer objects now that polling is halted
    for (NvComputer* computer : std::as_const(m_KnownHosts)) {
        delete computer;
    }
}

void DelayedFlushThread::run() {
    for (;;) {
        // Hosts stored by another process and unknown here: written back with ours, then
        // handed to the GUI thread, which adopts them (ComputerManager::adoptStoredHosts)
        QVector<NvComputer*> foreignHosts;

        // Wait for a delayed flush request or an interruption
        {
            QMutexLocker locker(&m_ComputerManager->m_DelayedFlushMutex);

            while (!QThread::currentThread()->isInterruptionRequested() && !m_ComputerManager->m_NeedsDelayedFlush) {
                m_ComputerManager->m_DelayedFlushCondition.wait(&m_ComputerManager->m_DelayedFlushMutex);
            }

            // Bail without flushing if we woke up for an interruption alone.
            // If we have both an interruption and a flush request, do the flush.
            if (!m_ComputerManager->m_NeedsDelayedFlush) {
                Q_ASSERT(QThread::currentThread()->isInterruptionRequested());
                break;
            }

            // Reset the delayed flush flag to ensure any racing saveHosts() call will set it again
            m_ComputerManager->m_NeedsDelayedFlush = false;

            // Another LavArtemis process (an art:// pairing link, `lavartemis pair`: there is no
            // single instance) may have stored hosts or certificates since we last read or wrote
            // the list: take them in instead of writing over them. Compared with what we last
            // stored, under the delayed flush mutex, before that snapshot is replaced below.
            foreignHosts = m_ComputerManager->mergeStoredHosts();

            // Update the last serialized hosts map under the delayed flush mutex
            m_ComputerManager->m_LastSerializedHosts.clear();
            for (const NvComputer* computer : std::as_const(m_ComputerManager->m_KnownHosts)) {
                // Copy the current state of the NvComputer to allow us to check later if we need
                // to serialize it again when attribute updates occur.
                QReadLocker computerLock(&computer->lock);
                m_ComputerManager->m_LastSerializedHosts[computer->uuid] = *computer;
            }
        }

        // Perform the flush
        {
            QSettings settings;

            // First, write to the backup location
            settings.beginWriteArray(SER_HOSTS_BACKUP);
            {
                QReadLocker lock(&m_ComputerManager->m_Lock);
                int i = 0;
                for (const NvComputer* computer : std::as_const(m_ComputerManager->m_KnownHosts)) {
                    settings.setArrayIndex(i++);
                    computer->serialize(settings, false);
                }
                for (const NvComputer* computer : std::as_const(foreignHosts)) {
                    settings.setArrayIndex(i++);
                    computer->serialize(settings, false);
                }
            }
            settings.endArray();

            // Next, write to the primary location
            settings.remove(SER_HOSTS);
            settings.beginWriteArray(SER_HOSTS);
            {
                QReadLocker lock(&m_ComputerManager->m_Lock);
                int i = 0;
                for (const NvComputer* computer : std::as_const(m_ComputerManager->m_KnownHosts)) {
                    settings.setArrayIndex(i++);
                    computer->serialize(settings, true);
                }
                for (const NvComputer* computer : std::as_const(foreignHosts)) {
                    settings.setArrayIndex(i++);
                    computer->serialize(settings, true);
                }
            }
            settings.endArray();

            // Finally, delete the backup copy
            settings.remove(SER_HOSTS_BACKUP);
        }

        if (!foreignHosts.isEmpty()) {
            ComputerManager* cm = m_ComputerManager;
            QMetaObject::invokeMethod(cm, [cm, foreignHosts] { cm->adoptStoredHosts(foreignHosts); },
                                      Qt::QueuedConnection);
        }
    }
}

// Called by the flush thread with m_DelayedFlushMutex held (it protects m_LastSerializedHosts
// and m_ForgottenUuids). Lock order: flush mutex, then m_Lock, then a computer's lock.
QVector<NvComputer*> ComputerManager::mergeStoredHosts()
{
    QVector<NvComputer*> foreign;
    QSettings settings;

    int count = settings.beginReadArray(SER_HOSTS);
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        NvComputer* stored = new NvComputer(settings);
        if (stored->uuid.isEmpty()) {
            delete stored;
            continue;
        }

        NvComputer* known;
        {
            QReadLocker lock(&m_Lock);
            known = m_KnownHosts.value(stored->uuid);
        }

        if (known != nullptr) {
            // What we stored last time: a different certificate on disk was written by another
            // process (it paired this host again). Unknown here yet: only fill an empty one.
            auto last = m_LastSerializedHosts.constFind(stored->uuid);
            bool storedElsewhere = !stored->serverCert.isNull() &&
                    (last != m_LastSerializedHosts.constEnd() ? last->serverCert != stored->serverCert : true);
            if (storedElsewhere) {
                QWriteLocker computerLock(&known->lock);
                if (known->serverCert != stored->serverCert &&
                        (last != m_LastSerializedHosts.constEnd() || known->serverCert.isNull())) {
                    qInfo() << "Taking the certificate of" << known->name << "stored by another LavArtemis process";
                    known->serverCert = stored->serverCert;
                    // Unknown until the next poll reads it over HTTPS with this certificate
                    known->pairState = NvComputer::PS_UNKNOWN;
                }
            }
            delete stored;
        }
        else if (!m_ForgottenUuids.contains(stored->uuid) && !m_LastSerializedHosts.contains(stored->uuid)) {
            // Added by another process since we last read or wrote the list
            qInfo() << "Taking" << stored->name << "added by another LavArtemis process";
            foreign.append(stored);
        }
        else {
            // Forgotten here, or a host we stored and then removed: our list wins
            delete stored;
        }
    }
    settings.endArray();

    return foreign;
}

// GUI thread: hosts that another process stored (mergeStoredHosts), now in our list too
void ComputerManager::adoptStoredHosts(QVector<NvComputer*> hosts)
{
    QVector<NvComputer*> added;
    {
        QMutexLocker flushLock(&m_DelayedFlushMutex);
        QWriteLocker lock(&m_Lock);
        for (NvComputer* computer : std::as_const(hosts)) {
            if (m_KnownHosts.contains(computer->uuid) || m_ForgottenUuids.contains(computer->uuid)) {
                delete computer;
                continue;
            }
            m_KnownHosts[computer->uuid] = computer;
            m_LastSerializedHosts[computer->uuid] = *computer;
            startPollingComputer(computer);
            added.append(computer);
        }
    }

    for (NvComputer* computer : std::as_const(added)) {
        emit computerStateChanged(computer);
    }
}

void ComputerManager::saveHosts()
{
    Q_ASSERT(m_DelayedFlushThread != nullptr && m_DelayedFlushThread->isRunning());

    // Punt to a worker thread because QSettings on macOS can take ages (> 500 ms)
    // to persist our host list to disk (especially when a host has a bunch of apps).
    QMutexLocker locker(&m_DelayedFlushMutex);
    m_NeedsDelayedFlush = true;
    m_DelayedFlushCondition.wakeOne();
}

QHostAddress ComputerManager::getBestGlobalAddressV6(QVector<QHostAddress> &addresses)
{
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv6Protocol) {
            if (address.isInSubnet(QHostAddress("fe80::"), 10)) {
                // Link-local
                continue;
            }

            if (address.isInSubnet(QHostAddress("fec0::"), 10)) {
                qInfo() << "Ignoring site-local address:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("fc00::"), 7)) {
                qInfo() << "Ignoring ULA:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("2002::"), 16)) {
                qInfo() << "Ignoring 6to4 address:" << address;
                continue;
            }

            if (address.isInSubnet(QHostAddress("2001::"), 32)) {
                qInfo() << "Ignoring Teredo address:" << address;
                continue;
            }

            return address;
        }
    }

    return QHostAddress();
}

void ComputerManager::startPolling()
{
    QWriteLocker lock(&m_Lock);

    if (++m_PollingRef > 1) {
        return;
    }

#ifdef LAV_TEST_BUILD
    // Test build: no discovery on the network, hosts are added by address only.
    const bool mdnsAllowed = false;
#else
    const bool mdnsAllowed = true;
#endif

    if (mdnsAllowed && m_Prefs->enableMdns) {
        // Start an MDNS query for GameStream hosts
        m_MdnsServer.reset(new QMdnsEngine::Server());
        m_MdnsBrowser = new QMdnsEngine::Browser(m_MdnsServer.data(), "_nvstream._tcp.local.");
        connect(m_MdnsBrowser, &QMdnsEngine::Browser::serviceAdded,
                this, [this](const QMdnsEngine::Service& service) {
            qInfo() << "Discovered mDNS host:" << service.hostname();

            MdnsPendingComputer* pendingComputer = new MdnsPendingComputer(m_MdnsServer, service);
            connect(pendingComputer, &MdnsPendingComputer::resolvedHost,
                    this, &ComputerManager::handleMdnsServiceResolved);
            m_PendingResolution.append(pendingComputer);
        });
    }
    else {
        qWarning() << "mDNS is disabled by user preference";
    }

    // Start polling threads for each known host
    QMapIterator<QString, NvComputer*> i(m_KnownHosts);
    while (i.hasNext()) {
        i.next();
        startPollingComputer(i.value());
    }
}

// Must hold m_Lock for write
void ComputerManager::startPollingComputer(NvComputer* computer)
{
    if (m_PollingRef == 0) {
        return;
    }

    // Not while it is being paired: finishPairing() starts its poller again
    if (computer->uuid == m_PairingUuid) {
        return;
    }

    ComputerPollingEntry* pollingEntry;

    if (!m_PollEntries.contains(computer->uuid)) {
        pollingEntry = m_PollEntries[computer->uuid] = new ComputerPollingEntry();
    }
    else {
        pollingEntry = m_PollEntries[computer->uuid];
    }

    if (!pollingEntry->isActive()) {
        PcMonitorThread* thread = new PcMonitorThread(computer);
        connect(thread, &PcMonitorThread::computerStateChanged,
                this, &ComputerManager::handleComputerStateChanged);
        pollingEntry->setActiveThread(thread);
        thread->start();
    }
}

void ComputerManager::handleMdnsServiceResolved(MdnsPendingComputer* computer,
                                                QVector<QHostAddress>& addresses)
{
    QHostAddress v6Global = getBestGlobalAddressV6(addresses);
    bool added = false;

    // Add the host using the IPv4 address
    for (const QHostAddress& address : std::as_const(addresses)) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            // NB: We don't just call addNewHost() here with v6Global because the IPv6
            // address may not be reachable (if the user hasn't installed the IPv6 helper yet
            // or if this host lacks outbound IPv6 capability). We want to add IPv6 even if
            // it's not currently reachable.
            addNewHost(NvAddress(address, computer->port()),
                       true, computer->hostname(),
                       NvAddress(v6Global, computer->port()));
            added = true;
            break;
        }
    }

    if (!added) {
        // If we get here, there wasn't an IPv4 address so we'll do it v6-only
        for (const QHostAddress& address : std::as_const(addresses)) {
            if (address.protocol() == QAbstractSocket::IPv6Protocol) {
                // Use a link-local or site-local address for the "local address"
                if (address.isInSubnet(QHostAddress("fe80::"), 10) ||
                        address.isInSubnet(QHostAddress("fec0::"), 10) ||
                        address.isInSubnet(QHostAddress("fc00::"), 7)) {
                    addNewHost(NvAddress(address, computer->port()),
                               true, computer->hostname(),
                               NvAddress(v6Global, computer->port()));
                    break;
                }
            }
        }
    }

    m_PendingResolution.removeOne(computer);
    computer->deleteLater();
}

void ComputerManager::saveHost(NvComputer *computer)
{
    // If no serializable properties changed, don't bother saving hosts
    QMutexLocker lock(&m_DelayedFlushMutex);
    QReadLocker computerLock(&computer->lock);
    if (!m_LastSerializedHosts.value(computer->uuid).isEqualSerialized(*computer)) {
        // Queue a request for a delayed flush to QSettings outside of the lock
        computerLock.unlock();
        lock.unlock();
        saveHosts();
    }
}

bool ComputerManager::isKnownHost(NvComputer* computer)
{
    QReadLocker lock(&m_Lock);
    for (const NvComputer* known : std::as_const(m_KnownHosts)) {
        if (known == computer) {
            return true;
        }
    }
    return false;
}

void ComputerManager::handleComputerStateChanged(NvComputer* computer)
{
    // A poll or an add that ended just before the host was forgotten: its signal was queued
    // before deleteHost() ran. The host has left the list; nothing may read or save it again.
    if (!isKnownHost(computer)) {
        return;
    }

    emit computerStateChanged(computer);

    if (computer->pendingQuit && computer->currentGameId == 0) {
        computer->pendingQuit = false;
        emit quitAppCompleted(QVariant());
    }

    // Save updates to this host
    saveHost(computer);
}

QVector<NvComputer*> ComputerManager::getComputers()
{
    QReadLocker lock(&m_Lock);

    // Return a sorted host list
    auto hosts = QVector<NvComputer*>::fromList(m_KnownHosts.values());
    std::stable_sort(hosts.begin(), hosts.end(), [](const NvComputer* host1, const NvComputer* host2) {
        return host1->name.toLower() < host2->name.toLower();
    });
    return hosts;
}

class DeferredHostDeletionTask : public QRunnable
{
public:
    DeferredHostDeletionTask(ComputerManager* cm, NvComputer* computer, ComputerPollingEntry* pollingEntry,
                             bool deleteComputer)
        : m_Computer(computer),
          m_Uuid(computer->uuid),
          m_ComputerManager(cm),
          m_PollingEntry(pollingEntry),
          m_DeleteComputer(deleteComputer) {}

    void run()
    {
        // Delete the polling entry first. This will stop all polling threads too.
        delete m_PollingEntry;

        // Delete cached box art (by uuid: m_Computer is not ours to read when a pairing deletes it)
        BoxArtManager::deleteBoxArt(m_Uuid);

        // Finally, delete the computer itself, on the GUI thread: after the signals its poller
        // queued there before it stopped (ComputerManager::handleComputerStateChanged drops them,
        // the host being out of the list). A pairing still using it deletes it when it ends.
        if (m_DeleteComputer) {
            NvComputer* computer = m_Computer;
            QMetaObject::invokeMethod(m_ComputerManager, [computer] { delete computer; }, Qt::QueuedConnection);
        }
    }

private:
    NvComputer* m_Computer;
    QString m_Uuid;
    ComputerManager* m_ComputerManager;
    ComputerPollingEntry* m_PollingEntry;
    bool m_DeleteComputer;
};

void ComputerManager::deleteHost(NvComputer* computer)
{
    // Out of the list now, on the GUI thread, so every model and the home see it gone at once.
    // It used to leave the list later, on a worker thread, without any signal: the home kept
    // showing it and its AppModel read the deleted object.
    ComputerPollingEntry* pollingEntry;
    QString uuid;
    {
        QWriteLocker lock(&m_Lock);
        uuid = computer->uuid;
        if (m_KnownHosts.value(uuid) != computer) {
            return;
        }
        pollingEntry = m_PollEntries.take(uuid);
        m_KnownHosts.remove(uuid);
    }
    {
        QMutexLocker lock(&m_DelayedFlushMutex);
        m_ForgottenUuids.insert(uuid);
    }

    // Persist the new host list with this computer deleted
    saveHosts();

    emit computerRemoved(computer);

    bool pairing = computer == m_PairingComputer;
    if (pairing) {
        m_DeleteAfterPairing.append(computer);
    }

    // Punt to a worker thread to avoid stalling the
    // UI while waiting for the polling thread to die
    QThreadPool::globalInstance()->start(new DeferredHostDeletionTask(this, computer, pollingEntry, !pairing));
}

void ComputerManager::renameHost(NvComputer* computer, QString name)
{
    {
        QWriteLocker lock(&computer->lock);

        computer->name = name;
        computer->hasCustomName = true;
    }

    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::clientSideAttributeUpdated(NvComputer* computer)
{
    // Notify the UI of the state change
    handleComputerStateChanged(computer);
}

void ComputerManager::handleAboutToQuit()
{
    QReadLocker lock(&m_Lock);

    // Interrupt polling threads immediately, so they
    // avoid making additional requests while quitting
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }
}

// The pairing errors, Android's sentences (HostsActivity, v2_pair_*): they name the host and never
// carry the host's raw XML or an English exception text (French in qml_fr.ts)
QString ComputerManager::pairingErrorText(const QString& name, PairingError error, const QString& detail)
{
    switch (error) {
    case PairingError::WrongPin:
        return tr("The code entered on %1 is not the right one. Try again: a new code will be shown.").arg(name);
    case PairingError::InGame:
        return tr("Pairing with %1 failed: a session is running on this PC. Quit it, then try again.").arg(name);
    case PairingError::Busy:
        return tr("A pairing is already under way on %1. Finish or cancel it in LavApollo, then try again.").arg(name);
    case PairingError::UnknownHost:
        return tr("The address of %1 cannot be found.").arg(name);
    case PairingError::NotFound:
        return tr("%1 did not answer the pairing request (error 404). Check that LavApollo is what runs on it.").arg(name);
    case PairingError::Network:
        return tr("The connection with %1 was lost during pairing (%2).").arg(name, detail);
    case PairingError::TimedOut:
        return tr("%1 did not get the code in time. Try again: a new code will be shown.").arg(name);
    case PairingError::LinkExpired:
        return tr("The link's code is no longer valid on %1: it works only once and expires after 3 minutes. Make a new link in LavApollo.").arg(name);
    case PairingError::Failed:
        break;
    }
    return tr("Pairing with %1 failed. Check that LavApollo runs on this PC, then try again.").arg(name);
}

QString ComputerManager::pairingErrorText(const QString& name, const GfeHttpResponseException& e)
{
    if (e.getStatusCode() == 404) {
        return pairingErrorText(name, PairingError::NotFound);
    }
    if (e.getStatusCode() == 503 && QString(e.getStatusMessage()).contains("OTP", Qt::CaseInsensitive)) {
        return pairingErrorText(name, PairingError::LinkExpired);
    }
    return pairingErrorText(name, PairingError::Network, tr("error %1").arg(e.getStatusCode()));
}

QString ComputerManager::pairingErrorText(const QString& name, const QtNetworkReplyException& e)
{
    switch (e.getError()) {
    case QNetworkReply::HostNotFoundError:
        return pairingErrorText(name, PairingError::UnknownHost);
    case QNetworkReply::ContentNotFoundError:
        return pairingErrorText(name, PairingError::NotFound);
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return pairingErrorText(name, PairingError::Network, tr("no answer in time"));
    case QNetworkReply::ConnectionRefusedError:
        return pairingErrorText(name, PairingError::Network, tr("connection refused"));
    case QNetworkReply::RemoteHostClosedError:
        return pairingErrorText(name, PairingError::Network, tr("connection closed by the PC"));
    case QNetworkReply::SslHandshakeFailedError:
        return pairingErrorText(name, PairingError::Network, tr("secure connection failed"));
    default:
        return pairingErrorText(name, PairingError::Network, tr("network error %1").arg((int)e.getError()));
    }
}

// How long the host may take before its user types the PIN (the first pairing request waits for
// it). Android waits forever; here a pairing left behind would block every other one.
#define PAIRING_PIN_WAIT_MS (3 * 60 * 1000)

class PendingPairingTask : public QRunnable
{
public:
    PendingPairingTask(ComputerManager* computerManager, NvComputer* computer, QString pin,
                       ComputerPollingEntry* pollingEntry)
        : m_ComputerManager(computerManager),
          m_Computer(computer),
          m_Pin(pin),
          m_PollingEntry(pollingEntry)
    {
    }

private:
    void run()
    {
        // No poll may run during pairing (Android's HostsActivity stops them the same way): the
        // poller's entry was taken out by beginPairing(); deleting it waits for its thread
        delete m_PollingEntry;

        QString name, appVersion;
        QSslCertificate serverCert;
        {
            QReadLocker lock(&m_Computer->lock);
            name = m_Computer->name;
            appVersion = m_Computer->appVersion;
            serverCert = m_Computer->serverCert;
        }

        QString error;
        QSslCertificate pairedCert;
        try {
            std::unique_ptr<NvPairingManager> pairingManager;
            {
                QReadLocker lock(&m_Computer->lock);
                pairingManager.reset(new NvPairingManager(m_Computer, PAIRING_PIN_WAIT_MS));
            }
            NvPairingManager::PairState result = pairingManager->pair(appVersion, m_Pin, serverCert);
            switch (result)
            {
            case NvPairingManager::PairState::PIN_WRONG:
                error = ComputerManager::pairingErrorText(name, ComputerManager::PairingError::WrongPin);
                break;
            case NvPairingManager::PairState::FAILED: {
                bool inGame;
                {
                    QReadLocker lock(&m_Computer->lock);
                    inGame = m_Computer->currentGameId != 0;
                }
                error = ComputerManager::pairingErrorText(name, inGame ? ComputerManager::PairingError::InGame
                                                                         : ComputerManager::PairingError::Failed);
                break;
            }
            case NvPairingManager::PairState::ALREADY_IN_PROGRESS:
                error = ComputerManager::pairingErrorText(name, ComputerManager::PairingError::Busy);
                break;
            case NvPairingManager::PairState::PAIRED:
                // Pinned by finishPairing(), on the GUI thread, under the computer's lock
                pairedCert = serverCert;
                break;
            }
        } catch (const GfeHttpResponseException& e) {
            qWarning() << "Pairing with" << name << "failed:" << e.toQString();
            error = ComputerManager::pairingErrorText(name, e);
        } catch (const QtNetworkReplyException& e) {
            qWarning() << "Pairing with" << name << "failed:" << e.toQString();
            // The first request waits for the PIN: its own time limit is the user's, not the network's
            error = e.getError() == QNetworkReply::TimeoutError
                    ? ComputerManager::pairingErrorText(name, ComputerManager::PairingError::TimedOut)
                    : ComputerManager::pairingErrorText(name, e);
        } catch (const std::exception& e) {
            qWarning() << "Pairing with" << name << "failed:" << e.what();
            error = ComputerManager::pairingErrorText(name, ComputerManager::PairingError::Failed);
        }

        ComputerManager* cm = m_ComputerManager;
        NvComputer* computer = m_Computer;
        QMetaObject::invokeMethod(cm, [cm, computer, error, pairedCert] {
            cm->finishPairing(computer, error, pairedCert);
        }, Qt::QueuedConnection);
    }

    ComputerManager* m_ComputerManager;
    NvComputer* m_Computer;
    QString m_Pin;
    ComputerPollingEntry* m_PollingEntry;
};

bool ComputerManager::beginPairing(NvComputer* computer, QString pin, ComputerPollingEntry*& pollingEntry)
{
    pollingEntry = nullptr;
    if (m_PairingComputer != nullptr) {
        qWarning() << "A pairing is already under way with" << m_PairingUuid;
        return false;
    }

    QWriteLocker lock(&m_Lock);
    if (m_KnownHosts.value(computer->uuid) != computer) {
        return false;
    }
    m_PairingComputer = computer;
    m_PairingUuid = computer->uuid;
    m_PairingPin = pin;

    // Its poller stops for the pairing (startPollingComputer() skips it meanwhile) and starts
    // again in finishPairing()
    pollingEntry = m_PollEntries.take(computer->uuid);
    if (pollingEntry != nullptr) {
        pollingEntry->interrupt();
    }
    return true;
}

void ComputerManager::finishPairing(NvComputer* computer, QString error, QSslCertificate pairedCert)
{
    Q_ASSERT(computer == m_PairingComputer);
    m_PairingComputer = nullptr;
    {
        QWriteLocker lock(&m_Lock);
        m_PairingUuid.clear();
    }
    m_PairingPin.clear();

    if (m_DeleteAfterPairing.removeOne(computer)) {
        // Forgotten during its pairing: out of the list already, nothing to update
        emit pairingCompleted(computer, error);
        delete computer;
        return;
    }

    if (error.isEmpty()) {
        {
            QWriteLocker lock(&computer->lock);
            if (!pairedCert.isNull()) {
                computer->serverCert = pairedCert;
            }
            computer->pairState = NvComputer::PS_PAIRED;
        }

        // Persist the newly pinned server certificate for this host
        saveHost(computer);
    }

    {
        QWriteLocker lock(&m_Lock);
        startPollingComputer(computer);
    }

    // The models see it paired before the pairing completes (the home only shows paired hosts)
    if (error.isEmpty()) {
        handleComputerStateChanged(computer);
    }
    emit pairingCompleted(computer, error);
}

bool ComputerManager::pairHost(NvComputer* computer, QString pin)
{
    ComputerPollingEntry* pollingEntry;
    if (!beginPairing(computer, pin, pollingEntry)) {
        return false;
    }

    // Punt to a worker thread to avoid stalling the
    // UI while waiting for pairing to complete
    QThreadPool::globalInstance()->start(new PendingPairingTask(this, computer, pin, pollingEntry));
    return true;
}

QString ComputerManager::pairingUuid()
{
    return m_PairingUuid;
}

QString ComputerManager::pairingPin()
{
    return m_PairingPin;
}

class PendingOTPPairingTask : public QRunnable
{

public:
    PendingOTPPairingTask(ComputerManager* computerManager, NvComputer* computer, QString pin, QString passphrase,
                          ComputerPollingEntry* pollingEntry)
        : m_ComputerManager(computerManager),
          m_Computer(computer),
          m_Pin(pin),
          m_Passphrase(passphrase),
          m_PollingEntry(pollingEntry)
    {
    }

private:
    // Helper functions for crypto operations
    QByteArray encryptAES(const QByteArray& plaintext, const QByteArray& key) {
        EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
        if (!cipher) {
            qCritical() << "Failed to create AES cipher context";
            return QByteArray();
        }
        
        QByteArray ciphertext(plaintext.size(), 0);
        int ciphertextLen;
        
        EVP_EncryptInit(cipher, EVP_aes_128_ecb(), reinterpret_cast<const unsigned char*>(key.data()), NULL);
        EVP_CIPHER_CTX_set_padding(cipher, 0);
        
        EVP_EncryptUpdate(cipher,
                          reinterpret_cast<unsigned char*>(ciphertext.data()),
                          &ciphertextLen,
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          plaintext.length());
        
        if (ciphertextLen != ciphertext.length()) {
            qCritical() << "AES encryption length mismatch";
            EVP_CIPHER_CTX_free(cipher);
            return QByteArray();
        }
        
        EVP_CIPHER_CTX_free(cipher);
        return ciphertext;
    }
    
    QByteArray decryptAES(const QByteArray& ciphertext, const QByteArray& key) {
        EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
        if (!cipher) {
            qCritical() << "Failed to create AES cipher context";
            return QByteArray();
        }
        
        QByteArray plaintext(ciphertext.size(), 0);
        int plaintextLen;
        
        EVP_DecryptInit(cipher, EVP_aes_128_ecb(), reinterpret_cast<const unsigned char*>(key.data()), NULL);
        EVP_CIPHER_CTX_set_padding(cipher, 0);
        
        EVP_DecryptUpdate(cipher,
                          reinterpret_cast<unsigned char*>(plaintext.data()),
                          &plaintextLen,
                          reinterpret_cast<const unsigned char*>(ciphertext.data()),
                          ciphertext.length());
        
        if (plaintextLen != plaintext.length()) {
qCritical() << "AES decryption length mismatch";
            EVP_CIPHER_CTX_free(cipher);
            return QByteArray();
        }
        
        EVP_CIPHER_CTX_free(cipher);
        return plaintext;
    }
    
    QByteArray signMessage(const QByteArray& message) {
        // Load private key from PEM format
        QByteArray privateKeyData = IdentityManager::get()->getPrivateKey();
        
        BIO* bio = BIO_new_mem_buf(privateKeyData.data(), -1);
        if (!bio) {
qCritical() << "Failed to create BIO for private key";
            return QByteArray();
        }
        
        EVP_PKEY* privateKey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
        BIO_free(bio);
        
        if (!privateKey) {
qCritical() << "Failed to load private key from PEM";
            return QByteArray();
        }
        
        EVP_MD_CTX* ctx = EVP_MD_CTX_create();
        if (!ctx) {
            qCritical() << "Failed to create signing context";
            EVP_PKEY_free(privateKey);
            return QByteArray();
        }
        
        if (EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, privateKey) != 1) {
qCritical() << "Failed to initialize signing";
            EVP_MD_CTX_destroy(ctx);
            EVP_PKEY_free(privateKey);
            return QByteArray();
        }
        
        if (EVP_DigestSignUpdate(ctx, message.data(), message.length()) != 1) {
qCritical() << "Failed to update signing";
            EVP_MD_CTX_destroy(ctx);
            EVP_PKEY_free(privateKey);
            return QByteArray();
        }
        
        size_t signatureLength = 0;
        if (EVP_DigestSignFinal(ctx, NULL, &signatureLength) != 1) {
            qCritical() << "Failed to get signature length";
            EVP_MD_CTX_destroy(ctx);
            EVP_PKEY_free(privateKey);
            return QByteArray();
        }
        
        QByteArray signature((int)signatureLength, 0);
        if (EVP_DigestSignFinal(ctx, reinterpret_cast<unsigned char*>(signature.data()), &signatureLength) != 1) {
qCritical() << "Failed to sign message";
            EVP_MD_CTX_destroy(ctx);
            EVP_PKEY_free(privateKey);
            return QByteArray();
        }
        
        EVP_MD_CTX_destroy(ctx);
        EVP_PKEY_free(privateKey);
        
        signature.resize((int)signatureLength);
        return signature;
    }
    
    // Perform the complete pairing handshake after OTP authentication, matching Android client behavior
    bool performFullPairingHandshake(NvHTTP& http, const QString& saltStr, const QString& otpHash, const QString& pin)
    {
        qDebug() << "PendingOTPPairingTask: Starting full pairing handshake";
        
        try {
            // Generate AES key from salt + PIN like Android does
            QByteArray saltBytes = QByteArray::fromHex(saltStr.toUtf8());
            QByteArray pinBytes = pin.toUtf8();
            QByteArray saltedPin = saltBytes + pinBytes;
            
            QCryptographicHash keyHasher(QCryptographicHash::Sha256);
            keyHasher.addData(saltedPin);
            QByteArray aesKey = keyHasher.result().left(16); // Take first 16 bytes for AES-128
            
qDebug() << "PendingOTPPairingTask: Generated AES key from salt+PIN";
            
            // Step 1: Generate random challenge and encrypt it with AES key
            QByteArray randomChallenge(16, 0);
            for (int i = 0; i < 16; i++) {
                randomChallenge[i] = QRandomGenerator::global()->bounded(256);
            }
            
            // Encrypt the challenge with AES-128-ECB (matches Android's encryptAes)
            QByteArray encryptedChallenge = encryptAES(randomChallenge, aesKey);
            
            qDebug() << "PendingOTPPairingTask: Sending encrypted client challenge";
            QString challengeParams = QString("clientchallenge=%1").arg(QString(encryptedChallenge.toHex().toUpper()));
            QString challengeResp = http.openConnectionToString(
                http.m_BaseUrlHttp,
                "pair",
                challengeParams + "&updateState=1",
                5000
            );
            
            if (!challengeResp.contains("<paired>1</paired>")) {
                qDebug() << "PendingOTPPairingTask: Client challenge failed";
                return false;
            }
            
            // Extract encrypted challenge response from server
            QRegularExpression challengeResponseRegex(R"(<challengeresponse>([A-Fa-f0-9]+)</challengeresponse>)");
            QRegularExpressionMatch challengeResponseMatch = challengeResponseRegex.match(challengeResp);
            if (!challengeResponseMatch.hasMatch()) {
                qDebug() << "PendingOTPPairingTask: No challenge response found";
                return false;
            }
            QString encChallengeResponseHex = challengeResponseMatch.captured(1);
            QByteArray encChallengeResponse = QByteArray::fromHex(encChallengeResponseHex.toUtf8());
            
            // Decrypt the challenge response and verify it (matches Android client behavior)
            QByteArray challengeResponseData = decryptAES(encChallengeResponse, aesKey);
            if (challengeResponseData.isEmpty()) {
                qDebug() << "PendingOTPPairingTask: Failed to decrypt challenge response";
                return false;
            }
            
            // Step 2: Send server challenge response
            // Generate a 16-byte random client secret like Android does
            QByteArray clientSecret(16, 0);
            for (int i = 0; i < 16; i++) {
                clientSecret[i] = QRandomGenerator::global()->bounded(256);
            }
            
            // Create challenge response hash following the Android implementation
            // Extract server response parts (hash + challenge from server)
            QByteArray serverResponse(challengeResponseData.data(), 32); // First 32 bytes are server hash
            QByteArray serverChallenge = challengeResponseData.mid(32, 16); // Next 16 bytes are server challenge
            
            // Get our client certificate signature for the challenge response
            QByteArray clientCert = IdentityManager::get()->getCertificate();
            QSslCertificate cert(clientCert);
            QByteArray clientCertDer = cert.toDer();
            const unsigned char* p = (const unsigned char*)clientCertDer.constData();
            X509* x509 = d2i_X509(NULL, &p, clientCertDer.length());
            if (!x509) {
                qWarning() << "PendingOTPPairingTask: Failed to parse client certificate";
                return false;
            }

            const ASN1_BIT_STRING* signature_asn1;
            const X509_ALGOR* sig_alg;
            X509_get0_signature(&signature_asn1, &sig_alg, x509);

            QByteArray certSignature;
            if (signature_asn1) {
                certSignature = QByteArray((const char*)ASN1_STRING_get0_data(signature_asn1), ASN1_STRING_length(signature_asn1));
            }
            X509_free(x509);
            
            // Build challenge response: serverChallenge + certSignature + clientSecret
            QByteArray challengeResponse;
            challengeResponse.append(serverChallenge);
            challengeResponse.append(certSignature);
            challengeResponse.append(clientSecret);
            
            QByteArray challengeRespHash = QCryptographicHash::hash(challengeResponse, QCryptographicHash::Sha256);
            challengeRespHash.resize(32); // Pad to 32 bytes like NvPairingManager
            
            // Encrypt the challenge response hash with AES
            QByteArray encryptedChallengeResp = encryptAES(challengeRespHash, aesKey);
            
            QString serverRespParams = QString("serverchallengeresp=%1").arg(QString(encryptedChallengeResp.toHex().toUpper()));
            QString serverResp = http.openConnectionToString(
                http.m_BaseUrlHttp,
                "pair",
                serverRespParams + "&updateState=1",
                5000
            );
            
            if (!serverResp.contains("<paired>1</paired>")) {
                qDebug() << "PendingOTPPairingTask: Server challenge response failed";
                return false;
            }
            
            // Step 3: Send client pairing secret with proper RSA signature
            qDebug() << "PendingOTPPairingTask: Sending client pairing secret";
            
            // Sign the client secret using proper RSA-SHA256 signature
            QByteArray signature = signMessage(clientSecret);
            if (signature.isEmpty()) {
                qDebug() << "PendingOTPPairingTask: Failed to sign client secret";
                return false;
            }
            
            // Create the client pairing secret exactly like NvPairingManager: clientSecret + signature
            QByteArray clientPairingSecret = clientSecret + signature;
            
            QString pairingSecretParams = QString("clientpairingsecret=%1").arg(QString(clientPairingSecret.toHex().toUpper()));
            QString secretResp = http.openConnectionToString(
                http.m_BaseUrlHttp,
                "pair",
                pairingSecretParams + "&updateState=1",
                5000
            );
            
            if (!secretResp.contains("<paired>1</paired>")) {
                qDebug() << "PendingOTPPairingTask: Client pairing secret failed";
                return false;
            }
            
            // Step 4: Final pairing challenge - use HTTPS like Android client does
            qDebug() << "PendingOTPPairingTask: Performing final pairing challenge";
            QString finalChallengeParams = QString("phrase=pairchallenge");
            QString finalResp = http.openConnectionToString(
                http.m_BaseUrlHttps,
                "pair",
                finalChallengeParams + "&updateState=1",
                5000
            );
            
            if (finalResp.contains("<paired>1</paired>")) {
                qDebug() << "PendingOTPPairingTask: Full pairing handshake completed successfully";
                return true;
            } else {
                qDebug() << "PendingOTPPairingTask: Final pairing challenge failed:" << finalResp;
                return false;
            }
            
        } catch (const GfeHttpResponseException& e) {
            qDebug() << "PendingOTPPairingTask: Pairing handshake HTTP error:" << e.getStatusMessage();
            return false;
        } catch (const QtNetworkReplyException& e) {
            qDebug() << "PendingOTPPairingTask: Pairing handshake network error:" << e.getErrorText();
            return false;
        } catch (const std::exception& e) {
            qDebug() << "PendingOTPPairingTask: Pairing handshake error:" << e.what();
            return false;
        }
    }
    
    // Ends on the GUI thread (ComputerManager::finishPairing), like the PIN pairing
    void done(const QString& error, const QSslCertificate& pairedCert = QSslCertificate())
    {
        ComputerManager* cm = m_ComputerManager;
        NvComputer* computer = m_Computer;
        QMetaObject::invokeMethod(cm, [cm, computer, error, pairedCert] {
            cm->finishPairing(computer, error, pairedCert);
        }, Qt::QueuedConnection);
    }

    void run()
    {
        // No poll during pairing (see PendingPairingTask)
        delete m_PollingEntry;

        QString name;
        std::unique_ptr<NvHTTP> httpHolder;
        {
            QReadLocker lock(&m_Computer->lock);
            name = m_Computer->name;
            httpHolder.reset(new NvHTTP(m_Computer));
        }
        NvHTTP& http = *httpHolder;
        const QString failed = ComputerManager::pairingErrorText(name, ComputerManager::PairingError::Failed);

        qInfo() << "PendingOTPPairingTask: Starting Apollo OTP pairing with" << name;

        try {
            // Generate a 16-byte salt
            QByteArray saltBytes(16, 0);
            for (int i = 0; i < 16; i++) {
                saltBytes[i] = QRandomGenerator::global()->bounded(256);
            }
            QString saltStr = saltBytes.toHex();

            // Generate the OTP hash (never logged: it is derived from the PIN and the passphrase)
            QString plainText = m_Pin + saltStr + m_Passphrase;
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(plainText.toUtf8());
            QString otpHash = hash.result().toHex().toUpper();

            QString pairingParams = QString("devicename=%1&updateState=1&phrase=getservercert&salt=%2&clientcert=%3&otpauth=%4")
                .arg(ComputerManager::clientDeviceName())
                .arg(saltStr)
                .arg(QString(IdentityManager::get()->getCertificate().toHex()))
                .arg(otpHash);

            QString pairingRequest = http.openConnectionToString(
                http.m_BaseUrlHttp,
                "pair",
                pairingParams,
                5000
            );

            if (pairingRequest.isEmpty()) {
                qWarning() << "PendingOTPPairingTask: no response";
                done(failed);
                return;
            }

            // The OTP expired or was used already
            if (pairingRequest.contains("OTP auth not available")) {
                qWarning() << "PendingOTPPairingTask: OTP not available";
                done(ComputerManager::pairingErrorText(name, ComputerManager::PairingError::LinkExpired));
                return;
            }

            if (!pairingRequest.contains("<root status_code=\"200\">") || !pairingRequest.contains("<paired>1</paired>")) {
                qWarning() << "PendingOTPPairingTask: OTP pairing refused:" << pairingRequest;
                done(failed);
                return;
            }

            // Extract the server certificate from the response
            QRegularExpression certRegex(R"(<plaincert>([A-Fa-f0-9]+)</plaincert>)");
            QRegularExpressionMatch certMatch = certRegex.match(pairingRequest);
            if (!certMatch.hasMatch()) {
                qWarning() << "PendingOTPPairingTask: no server certificate in the answer";
                done(failed);
                return;
            }

            QSslCertificate serverCert(QByteArray::fromHex(certMatch.captured(1).toUtf8()));
            if (serverCert.isNull()) {
                qWarning() << "PendingOTPPairingTask: invalid server certificate";
                done(failed);
                return;
            }

            // HTTPS for the end of the handshake with this certificate; the host's is pinned by
            // finishPairing(), under the computer's lock, only once the handshake succeeded
            http.setServerCert(serverCert);
            if (!performFullPairingHandshake(http, saltStr, otpHash, m_Pin)) {
                // A wrong code or passphrase fails here (the host answered the first step anyway)
                qWarning() << "PendingOTPPairingTask: handshake failed";
                done(failed);
                return;
            }

            qInfo() << "PendingOTPPairingTask: paired with" << name;
            done(QString(), serverCert);
        } catch (const GfeHttpResponseException& e) {
            qWarning() << "PendingOTPPairingTask: HTTP error:" << e.toQString();
            done(ComputerManager::pairingErrorText(name, e));
        } catch (const QtNetworkReplyException& e) {
            qWarning() << "PendingOTPPairingTask: network error:" << e.toQString();
            done(ComputerManager::pairingErrorText(name, e));
        } catch (const std::exception& e) {
            qWarning() << "PendingOTPPairingTask: error:" << e.what();
            done(failed);
        }
    }

    ComputerManager* m_ComputerManager;
    NvComputer* m_Computer;
    QString m_Pin;
    QString m_Passphrase;
    ComputerPollingEntry* m_PollingEntry;
};

bool ComputerManager::pairHostWithOTP(NvComputer* computer, QString pin, QString passphrase)
{
    ComputerPollingEntry* pollingEntry;
    if (!beginPairing(computer, QString(), pollingEntry)) {
        return false;
    }

    // Punt to a worker thread to avoid stalling the
    // UI while waiting for OTP pairing to complete
    QThreadPool::globalInstance()->start(new PendingOTPPairingTask(this, computer, pin, passphrase, pollingEntry));
    return true;
}

// The name this PC gives the host when it pairs (the host lists its clients by it), as in
// serverinfo and like Android (its device model): the old "roth" made LavApollo list every
// LavArtemis PC as « Legacy Moonlight Client »
QString ComputerManager::clientDeviceName()
{
    QString deviceName = QSysInfo::machineHostName();
    if (deviceName.isEmpty()) {
        deviceName = "LavArtemis";
    }
    return QString::fromLatin1(QUrl::toPercentEncoding(deviceName));
}

class PendingQuitTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingQuitTask(ComputerManager* computerManager, NvComputer* computer)
        : m_Computer(computer)
    {
        connect(this, &PendingQuitTask::quitAppFailed,
                computerManager, &ComputerManager::quitAppCompleted);
    }

signals:
    void quitAppFailed(QString error);

private:
    void run()
    {
        NvHTTP http(m_Computer);

        try {
            if (m_Computer->currentGameId != 0) {
                http.quitApp();
            }
        } catch (const GfeHttpResponseException& e) {
            {
                QWriteLocker lock(&m_Computer->lock);
                m_Computer->pendingQuit = false;
            }
            if (e.getStatusCode() == 599) {
                // 599 is a special code we make a custom message for
                emit quitAppFailed(tr("The running game wasn't started by this PC. "
                                      "You must quit the game on the host PC manually or use the device that originally started the game."));
            }
            else {
                emit quitAppFailed(e.toQString());
            }
        } catch (const QtNetworkReplyException& e) {
            {
                QWriteLocker lock(&m_Computer->lock);
                m_Computer->pendingQuit = false;
            }
            emit quitAppFailed(e.toQString());
        }
    }

    NvComputer* m_Computer;
};

void ComputerManager::quitRunningApp(NvComputer* computer)
{
    QWriteLocker lock(&computer->lock);
    computer->pendingQuit = true;

    PendingQuitTask* quit = new PendingQuitTask(this, computer);
    QThreadPool::globalInstance()->start(quit);
}

void ComputerManager::stopPollingAsync()
{
    QWriteLocker lock(&m_Lock);

    Q_ASSERT(m_PollingRef > 0);
    if (--m_PollingRef > 0) {
        return;
    }

    // Delete machines that haven't been resolved yet
    while (!m_PendingResolution.isEmpty()) {
        MdnsPendingComputer* computer = m_PendingResolution.first();
        computer->deleteLater();
        m_PendingResolution.removeFirst();
    }

    // Delete the browser and server to stop discovery and refresh polling
    delete m_MdnsBrowser;
    m_MdnsBrowser = nullptr;
    m_MdnsServer.reset();

    // Interrupt all threads, but don't wait for them to terminate
    for (ComputerPollingEntry* entry : std::as_const(m_PollEntries)) {
        entry->interrupt();
    }
}

QString ComputerManager::testGuardRefusal(QString address)
{
#ifdef LAV_TEST_BUILD
    QString host;
    int port = DEFAULT_HTTP_PORT;
    QUrl url = QUrl::fromUserInput("art://" + address);
    if (url.isValid() && !url.host().isEmpty() && url.scheme() == "art") {
        host = url.host();
        port = url.port(DEFAULT_HTTP_PORT);
    }
    else {
        host = address;
    }

    if (!TestGuard::isAllowedAddress(host, (quint16)port)) {
        qWarning() << "Test build: manual add of" << address << "refused (loopback and ports 40000-44999 only)";
        return TestGuard::refusalMessage();
    }
#else
    Q_UNUSED(address);
#endif
    return QString();
}

void ComputerManager::addNewHostManually(QString address)
{
#ifdef LAV_TEST_BUILD
    if (!testGuardRefusal(address).isEmpty()) {
        emit computerAddCompleted(false, false, QVariant(), address);
        return;
    }
#endif

    NvAddress parsed;
    if (parseManualAddress(address, parsed)) {
        addNewHost(parsed, false, QString(), NvAddress(), address);
    }
    else {
        emit computerAddCompleted(false, false, QVariant(), address);
    }
}

bool ComputerManager::parseManualAddress(QString address, NvAddress& result)
{
    address = address.trimmed();
    if (address.isEmpty()) {
        return false;
    }

    QUrl url = QUrl::fromUserInput("art://" + address);
    if (url.isValid() && !url.host().isEmpty() && url.scheme() == "art" &&
            url.path().isEmpty() && !url.hasQuery() && !url.hasFragment() && url.userInfo().isEmpty()) {
        // If there wasn't a port specified, use the default
        result = NvAddress(url.host(), url.port(DEFAULT_HTTP_PORT));
        return true;
    }
    else if (QHostAddress(address).protocol() == QAbstractSocket::IPv6Protocol) {
        // The user specified an IPv6 literal without URL escaping, so use the default port
        result = NvAddress(address, DEFAULT_HTTP_PORT);
        return true;
    }
    return false;
}

bool ComputerManager::isValidManualAddress(QString address)
{
    NvAddress parsed;
    return parseManualAddress(address, parsed);
}

// Android's AddComputerManually.isWrongSubnetSiteLocalAddress(): a private IPv4 address (RFC 1918)
// that is in none of this PC's IPv4 networks -- the usual typo, or another network entirely
bool ComputerManager::isWrongSubnetAddress(QString address)
{
    NvAddress parsed;
    if (!parseManualAddress(address, parsed)) {
        return false;
    }
    QHostAddress target(parsed.address());
    if (target.protocol() != QAbstractSocket::IPv4Protocol ||
            !(target.isInSubnet(QHostAddress("10.0.0.0"), 8) ||
              target.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
              target.isInSubnet(QHostAddress("192.168.0.0"), 16))) {
        return false;
    }

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && entry.prefixLength() > 0 &&
                    target.isInSubnet(entry.ip(), entry.prefixLength())) {
                return false;
            }
        }
    }
    return true;
}

class PendingAddTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    PendingAddTask(ComputerManager* computerManager, QString name, NvAddress address, NvAddress mdnsIpv6Address, bool mdns,
                   QString userInput)
        : m_ComputerManager(computerManager),
          m_Name(name),
          m_Address(address),
          m_MdnsIpv6Address(mdnsIpv6Address),
          m_Mdns(mdns),
          m_AboutToQuit(false),
          m_UserInput(userInput)
    {
        connect(this, &PendingAddTask::computerAddCompleted,
                computerManager, &ComputerManager::computerAddCompleted);
        connect(this, &PendingAddTask::computerStateChanged,
                computerManager, &ComputerManager::handleComputerStateChanged);
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                this, &PendingAddTask::handleAboutToQuit);
    }

signals:
    void computerAddCompleted(QVariant success, QVariant detectedPortBlocking, QVariant uuid, QVariant address);

    void computerStateChanged(NvComputer* computer);

private:
    void handleAboutToQuit()
    {
        m_AboutToQuit = true;
    }

    QString fetchServerInfo(NvHTTP& http)
    {
        QString serverInfo;

        // Do nothing if we're quitting
        if (m_AboutToQuit) {
            return QString();
        }

        try {
            // There's a race condition between GameStream servers reporting presence over
            // mDNS and the HTTPS server being ready to respond to our queries. To work
            // around this issue, we will issue the request again after a few seconds if
            // we see a ServiceUnavailableError error.
            try {
                serverInfo = http.getServerInfo(NvHTTP::NVLL_VERBOSE);
            } catch (const QtNetworkReplyException& e) {
                if (e.getError() == QNetworkReply::ServiceUnavailableError) {
                    qWarning() << "Retrying request in 5 seconds after ServiceUnavailableError";
                    QThread::sleep(5);
                    serverInfo = http.getServerInfo(NvHTTP::NVLL_VERBOSE);
                    qInfo() << "Retry successful";
                }
                else {
                    // Rethrow other errors
                    throw e;
                }
            }
            return serverInfo;
        } catch (...) {
            if (!m_Mdns) {
                unsigned int portTestResult;

#ifdef LAV_TEST_BUILD
                // Test build: no connectivity probe to the Moonlight test server
                portTestResult = ML_TEST_RESULT_INCONCLUSIVE;
#else
                if (m_ComputerManager->m_Prefs->detectNetworkBlocking) {
                    // We failed to connect to the specified PC. Let's test to make sure this network
                    // isn't blocking Moonlight, so we can tell the user about it.
                    portTestResult = LiTestClientConnectivity("qt.conntest.moonlight-stream.org", 443,
                                                              ML_PORT_FLAG_TCP_47984 | ML_PORT_FLAG_TCP_47989);
                }
                else {
                    portTestResult = 0;
                }
#endif

                emit computerAddCompleted(false, portTestResult != 0 && portTestResult != ML_TEST_RESULT_INCONCLUSIVE, QVariant(), m_UserInput);
            }
            return QString();
        }
    }

    void run()
    {
        // Use the placeholder UID for the initial poll, then we'll switch to the real one if it's not GFE
        NvHTTP http(m_Address, 0, QSslCertificate(), false);

        if (m_Mdns) {
            if (m_MdnsIpv6Address.isNull()) {
                qInfo() << "Processing new PC" << m_Name << "from mDNS with local address" << m_Address.toString();
            }
            else {
                qInfo() << "Processing new PC" << m_Name << "from mDNS with local address" << m_Address.toString()
                        << "and IPv6 address" << m_MdnsIpv6Address.toString();
            }
        }
        else {
            qInfo() << "Processing new PC at" << m_Address.toString() << "from user";
        }

        // Perform initial serverinfo fetch over HTTP since we don't know which cert to use
        QString serverInfo = fetchServerInfo(http);
        if (serverInfo.isEmpty() && !m_MdnsIpv6Address.isNull()) {
            // Retry using the global IPv6 address if the IPv4 or link-local IPv6 address fails
            http.setAddress(m_MdnsIpv6Address);
            serverInfo = fetchServerInfo(http);
        }
        if (serverInfo.isEmpty()) {
            return;
        }

        // Create initial newComputer using HTTP serverinfo with no pinned cert
        NvComputer* newComputer = new NvComputer(http, serverInfo);
        http.setTrueUid(!newComputer->isNvidiaServerSoftware);

        // Check if we have a record of this host UUID to pull the pinned cert
        NvComputer* existingComputer;
        {
            QReadLocker lock(&m_ComputerManager->m_Lock);
            existingComputer = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);
            if (existingComputer != nullptr) {
                QReadLocker computerLock(&existingComputer->lock);
                http.setServerCert(existingComputer->serverCert);
            }
        }

        // Fetch serverinfo again over HTTPS with the pinned cert
        if (existingComputer != nullptr) {
            Q_ASSERT(http.httpsPort() != 0);
            serverInfo = fetchServerInfo(http);
            if (serverInfo.isEmpty()) {
                return;
            }

            // Update the polled computer with the HTTPS information
            NvComputer httpsComputer(http, serverInfo);
            newComputer->update(httpsComputer);
        }

        // Update addresses depending on the context
        if (m_Mdns) {
            // Only update local address if we actually reached the PC via this address.
            // If we reached it via the IPv6 address after the local address failed,
            // don't store the non-working local address.
            if (http.address() == m_Address) {
                newComputer->localAddress = m_Address;
            }

            // Get the WAN IP address using STUN if we're on mDNS over IPv4
#ifndef LAV_TEST_BUILD
            if (QHostAddress(newComputer->localAddress.address()).protocol() == QAbstractSocket::IPv4Protocol) {
                quint32 addr;
                int err = LiFindExternalAddressIP4("stun.moonlight-stream.org", 3478, &addr);
                if (err == 0) {
                    newComputer->setRemoteAddress(QHostAddress(qFromBigEndian(addr)));
                }
                else {
                    qWarning() << "STUN failed to get WAN address:" << err;
                }
            }
#endif

            if (!m_MdnsIpv6Address.isNull()) {
                Q_ASSERT(QHostAddress(m_MdnsIpv6Address.address()).protocol() == QAbstractSocket::IPv6Protocol);
                newComputer->ipv6Address = m_MdnsIpv6Address;
            }
        }
        else {
            newComputer->manualAddress = m_Address;
        }

        QHostAddress hostAddress(m_Address.address());
        bool addressIsSiteLocalV4 =
                hostAddress.isInSubnet(QHostAddress("10.0.0.0"), 8) ||
                hostAddress.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
                hostAddress.isInSubnet(QHostAddress("192.168.0.0"), 16);

        {
            // Check if this PC already exists using opportunistic read lock
            m_ComputerManager->m_Lock.lockForRead();
            NvComputer* existingComputer = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);

            // If it doesn't already exist, convert to a write lock in preparation for updating.
            //
            // NB: ComputerManager's lock protects the host map itself, not the elements inside.
            // Those are protected by their individual locks. Since we only mutate the map itself
            // when the PC doesn't exist, we need the lock in write-mode for that case only.
            if (existingComputer == nullptr) {
                m_ComputerManager->m_Lock.unlock();
                m_ComputerManager->m_Lock.lockForWrite();

                // Since we had to unlock to lock for write, someone could have raced and added
                // this PC before us. We have to check again whether it already exists.
                existingComputer = m_ComputerManager->m_KnownHosts.value(newComputer->uuid);
            }

            if (existingComputer != nullptr) {
                // Fold it into the existing PC
                bool changed = existingComputer->update(*newComputer);
                QString uuid = newComputer->uuid;
                delete newComputer;

                // Drop the lock before notifying
                m_ComputerManager->m_Lock.unlock();

                // Tell our client if something changed
                if (changed) {
                    qInfo() << existingComputer->name << "is now at" << existingComputer->activeAddress.toString();
                    emit computerStateChanged(existingComputer);
                }

                // For non-mDNS clients, let them know it succeeded, and which host answered
                if (!m_Mdns) {
                    emit computerAddCompleted(true, false, uuid, m_UserInput);
                }
            }
            else {
                // Store this in our active sets
                m_ComputerManager->m_KnownHosts[newComputer->uuid] = newComputer;

                // Start polling if enabled (write lock required)
                m_ComputerManager->startPollingComputer(newComputer);

                // The uuid the add completes with is copied under the lock, like the branch above: once
                // it drops, the poller and the GUI thread (which may forget the PC) share newComputer.
                QString uuid = newComputer->uuid;

                // Drop the lock before notifying
                m_ComputerManager->m_Lock.unlock();

                // If this wasn't added via mDNS but it is a RFC 1918 IPv4 address and not a VPN,
                // go ahead and do the STUN request now to populate an external address.
#ifndef LAV_TEST_BUILD
                if (!m_Mdns && addressIsSiteLocalV4 && newComputer->getActiveAddressReachability() != NvComputer::RI_VPN) {
                    quint32 addr;
                    int err = LiFindExternalAddressIP4("stun.moonlight-stream.org", 3478, &addr);
                    if (err == 0) {
                        newComputer->setRemoteAddress(QHostAddress(qFromBigEndian(addr)));
                    }
                    else {
                        qWarning() << "STUN failed to get WAN address:" << err;
                    }
                }
#endif

                // Tell our client about this new PC. First: both signals are queued to the GUI
                // thread in this order, so the models have the new row when the add completes
                // (HostsView looks the host up there to pair it).
                emit computerStateChanged(newComputer);

                // For non-mDNS clients, let them know it succeeded, and which host answered
                if (!m_Mdns) {
                    emit computerAddCompleted(true, false, uuid, m_UserInput);
                }
            }
        }
    }

    ComputerManager* m_ComputerManager;
    QString m_Name;
    NvAddress m_Address;
    NvAddress m_MdnsIpv6Address;
    bool m_Mdns;
    bool m_AboutToQuit;
    QString m_UserInput;
};

void ComputerManager::addNewHost(NvAddress address, bool mdns, QString name, NvAddress mdnsIpv6Address, QString userInput)
{
    // Punt to a worker thread to avoid stalling the
    // UI while waiting for serverinfo query to complete
    PendingAddTask* addTask = new PendingAddTask(this, name, address, mdnsIpv6Address, mdns, userInput);
    QThreadPool::globalInstance()->start(addTask);
}

QString ComputerManager::generatePinString()
{
    return QString::asprintf("%04u", QRandomGenerator::system()->bounded(10000));
}

#include "computermanager.moc"
