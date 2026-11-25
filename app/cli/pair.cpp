#include "pair.h"

#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include <QCoreApplication>
#include <QTimer>

#define COMPUTER_SEEK_TIMEOUT 10000

namespace CliPair
{

enum State {
    StateInit,
    StateSeekComputer,
    StatePairing,
    StateFailure,
    StateComplete,
};

class Event
{
public:
    enum Type {
        ComputerFound,
        Executed,
        PairingCompleted,
        Timedout,
    };

    Event(Type type)
        : type(type), computerManager(nullptr), computer(nullptr) {}

    Type type;
    ComputerManager *computerManager;
    NvComputer *computer;
    QString errorMessage;
};

class LauncherPrivate
{
    Q_DECLARE_PUBLIC(Launcher)

public:
    LauncherPrivate(Launcher *q) : q_ptr(q) {}

    void handleEvent(Event event)
    {
        Q_Q(Launcher);

        switch (event.type) {
        // Occurs when CliPair becomes visible and the UI calls launcher's execute()
        case Event::Executed:
            if (m_State == StateInit) {
                m_State = StateSeekComputer;
                m_ComputerManager = event.computerManager;
                q->connect(m_ComputerManager, &ComputerManager::pairingCompleted,
                           q, &Launcher::onPairingCompleted);

                // If we weren't provided a predefined PIN, generate one now
                if (m_PredefinedPin.isEmpty()) {
                    m_PredefinedPin = m_ComputerManager->generatePinString();
                }

                m_ComputerSeeker = new ComputerSeeker(m_ComputerManager, m_ComputerName, q);
                q->connect(m_ComputerSeeker, &ComputerSeeker::computerFound,
                           q, &Launcher::onComputerFound);
                q->connect(m_ComputerSeeker, &ComputerSeeker::errorTimeout,
                           q, &Launcher::onTimeout);
                m_ComputerSeeker->start(COMPUTER_SEEK_TIMEOUT);

                emit q->searchingComputer(m_ComputerName);
            }
            break;
        // Occurs when searched computer is found
        case Event::ComputerFound:
            if (m_State == StateSeekComputer) {
                m_Computer = event.computer;
                if (event.computer->pairState == NvComputer::PS_PAIRED) {
                    // Already paired: that is what the link asked for, so it succeeded and the
                    // PC opens (Android's HostsActivity: getPairState() == PAIRED -> success)
                    m_State = StateComplete;
                    emit q->success(event.computer->uuid, event.computer->name);
                }
                else {
                    Q_ASSERT(!m_PredefinedPin.isEmpty());

                    // A passphrase means Apollo's OTP pairing, where the PIN is
                    // combined with it rather than typed on the host.
                    bool started;
                    if (m_Passphrase.isEmpty()) {
                        started = m_ComputerManager->pairHost(event.computer, m_PredefinedPin);
                    }
                    else {
                        started = m_ComputerManager->pairHostWithOTP(event.computer, m_PredefinedPin, m_Passphrase);
                    }

                    if (!started) {
                        m_State = StateFailure;
                        emit q->failed(ComputerManager::pairingErrorText(event.computer->name,
                                                                         ComputerManager::PairingError::Busy));
                    }
                    else {
                        m_State = StatePairing;
                        emit q->pairing(event.computer->name, m_PredefinedPin, !m_Passphrase.isEmpty());
                    }
                }
            }
            break;
        // Occurs when pairing operation completes
        case Event::PairingCompleted:
            // Only the pairing this launcher started
            if (m_State == StatePairing && event.computer == m_Computer) {
                if (event.errorMessage.isEmpty()) {
                    m_State = StateComplete;
                    emit q->success(event.computer->uuid, event.computer->name);
                }
                else {
                    m_State = StateFailure;
                    emit q->failed(event.errorMessage);
                }
            }
            break;
        // Occurs when computer search timed out
        case Event::Timedout:
            if (m_State == StateSeekComputer) {
                m_State = StateFailure;
                QString reason = m_ComputerSeeker->errorMessage();
                // Same sentence as « Ajouter un PC » (v2_add_unreachable)
                emit q->failed(reason.isEmpty() ? Launcher::tr("Unable to reach %1. Check that the PC is on, that LavApollo is running and that the address is right.").arg(m_ComputerName) : reason);
            }
            break;
        }
    }

    Launcher *q_ptr;
    QString m_ComputerName;
    QString m_PredefinedPin;
    QString m_Passphrase;
    ComputerManager *m_ComputerManager;
    ComputerSeeker *m_ComputerSeeker;
    NvComputer *m_Computer;
    State m_State;
    QTimer *m_TimeoutTimer;
};

Launcher::Launcher(QString computer, QString predefinedPin, QString passphrase, QObject *parent)
    : QObject(parent),
      m_DPtr(new LauncherPrivate(this))
{
    Q_D(Launcher);
    d->m_ComputerName = computer;
    d->m_PredefinedPin = predefinedPin;
    d->m_Passphrase = passphrase;
    d->m_State = StateInit;
    d->m_Computer = nullptr;
    d->m_TimeoutTimer = new QTimer(this);
    d->m_TimeoutTimer->setSingleShot(true);
    connect(d->m_TimeoutTimer, &QTimer::timeout,
            this, &Launcher::onTimeout);
}

Launcher::~Launcher()
{
}

void Launcher::execute(ComputerManager *manager)
{
    Q_D(Launcher);
    Event event(Event::Executed);
    event.computerManager = manager;
    d->handleEvent(event);
}

bool Launcher::isExecuted() const
{
    Q_D(const Launcher);
    return d->m_State != StateInit;
}

void Launcher::onComputerFound(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerFound);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onTimeout()
{
    Q_D(Launcher);
    Event event(Event::Timedout);
    d->handleEvent(event);
}

void Launcher::onPairingCompleted(NvComputer* computer, QString error)
{
    Q_D(Launcher);
    Event event(Event::PairingCompleted);
    event.computer = computer;
    event.errorMessage = error;
    d->handleEvent(event);
}

}
