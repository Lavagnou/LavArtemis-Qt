#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>

class NvComputer;

// Declare opaque pointers for Qt's meta-object system
#ifndef NVCOMPUTER_OPAQUE_DECLARED
#define NVCOMPUTER_OPAQUE_DECLARED
Q_DECLARE_OPAQUE_POINTER(NvComputer*)
#endif

/**
 * @brief The host's commands (Apollo / LavApollo `server_cmd`) in the in-game menu.
 *
 * As on Android (GameMenu.showServerCmd): the list is the host's `ServerCommand` entries of
 * serverinfo, which the host sends only to a client that has the « Commande de serveur »
 * permission; a command runs by its index over the control stream (LiSendExecServerCmd). An
 * empty list is explained by the menu (« Aucune commande de serveur »), never guessed.
 */
class ServerCommandManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ServerCommandManager(QObject *parent = nullptr);

    // The stream's start (main thread): the commands the host listed when it was last polled
    void setConnection(NvComputer *computer);

    // Re-reads the list (the menu opens) and emits commandsRefreshed()
    Q_INVOKABLE void refreshCommands();

    Q_INVOKABLE QStringList getAvailableCommands() const { return m_availableCommands; }

    // Runs one of them on the host; commandExecuted() or commandFailed() tells how it went
    Q_INVOKABLE void executeCommand(const QString &commandName);

signals:
    void commandsRefreshed();
    void commandExecuted(const QString& commandName);
    void commandFailed(const QString& commandName, const QString& error);

private:
    NvComputer *m_computer;
    QStringList m_availableCommands;
};
