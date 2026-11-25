#include "servercommandmanager.h"
#include "nvcomputer.h"
#include "streaming/session.h"

#include <QReadLocker>
#include <Limelight.h>

ServerCommandManager::ServerCommandManager(QObject *parent)
    : QObject(parent)
    , m_computer(nullptr)
{
}

void ServerCommandManager::setConnection(NvComputer *computer)
{
    m_computer = computer;
    refreshCommands();
}

void ServerCommandManager::refreshCommands()
{
    m_availableCommands.clear();
    if (m_computer != nullptr) {
        QReadLocker lock(&m_computer->lock);
        m_availableCommands = m_computer->serverCommands;
    }
    emit commandsRefreshed();
}

void ServerCommandManager::executeCommand(const QString &commandName)
{
    // The host runs a command by its index in the list it sent
    const int index = m_availableCommands.indexOf(commandName);
    if (index < 0) {
        emit commandFailed(commandName, tr("the PC doesn't offer this command"));
        return;
    }

    // Sent over the stream's control connection: only while the stream runs
    Session* session = Session::get();
    if (session == nullptr || !session->isStreamLoopRunning()) {
        emit commandFailed(commandName, tr("the stream is over"));
        return;
    }

    const int result = LiSendExecServerCmd(static_cast<uint8_t>(index));
    if (result == 0) {
        emit commandExecuted(commandName);
    }
    else {
        emit commandFailed(commandName, tr("error %1").arg(result));
    }
}
