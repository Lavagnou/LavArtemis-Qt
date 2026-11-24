#pragma once

// Test build only (CONFIG+=lav_test): the testbench command channel. A QLocalServer named
// "lavartemis-test" (\\.\pipe\lavartemis-test on Windows, current user only, one client at a
// time) takes one JSON request per line and answers one JSON line each. Every command runs on
// the GUI thread, which also runs the stream's SDL loop: Qt keeps dispatching this socket's
// events while that loop pumps Windows messages. Commands: CLAUDE.md, "Canal de commande".

#include "testprotocol.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <functional>

class QLocalServer;
class QLocalSocket;
class QQmlApplicationEngine;
class QQuickWindow;
class QTimer;
class Session;
struct _SDL_Joystick;
struct SDL_Window;

class TestControl : public QObject
{
    Q_OBJECT

public:
    static constexpr const char* kServerName = "lavartemis-test";

    TestControl(QQmlApplicationEngine* engine, QObject* parent = nullptr);
    ~TestControl() override;

    // Starts listening; false (and a log line) if the name is taken
    bool start();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();

private:
    void registerCommands();
    void sendLine(QLocalSocket* socket, const QByteArray& line);
    void shutdown();

    // Requests run one at a time from a queued call, never from readyRead and never inside
    // another command (a nested event loop delivers runNext(), which then waits)
    void scheduleNext();
    void runNext();

    QQuickWindow* mainWindow() const;
    SDL_Window* streamWindow() const;
    bool quickMenuVisible() const;
    // key/text/wheel: "target" is "auto" (the stream while it runs and the quick menu is
    // closed, else Qt), "qt" or "stream"
    bool targetsStream(const QJsonObject& args, QString& error) const;
    QQuickWindow* qtWindowArg(const QJsonObject& args, QString& error) const;

    bool cmdTree(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdClick(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdKey(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdText(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdWheel(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdMouse(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdGamepad(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdScreenshot(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdWindows(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdState(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdSetting(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdReset(const QJsonObject& args, QJsonObject& result, QString& error);
    bool cmdQuit(const QJsonObject& args, QJsonObject& result, QString& error);

    // Stream input (SDL_PushEvent into the session's SDL loop)
    bool pushKey(SDL_Window* window, const TestProtocol::KeySpec& key, int modifiers, int state, QString& error);
    bool pushText(SDL_Window* window, const QString& text, QString& error);
    bool pushMouseMotion(SDL_Window* window, int x, int y, QString& error);
    bool pushMouseButton(SDL_Window* window, int button, bool down, QString& error);
    bool pushWheel(SDL_Window* window, double dx, double dy, QString& error);
    void syncMouseWindow(SDL_Window* window);

    // The virtual gamepad ("LavArtemis Test Pad"), attached during a stream
    bool attachPad(QString& error);
    void detachPad();
    void checkPadSession();

    void beginQuit();

    QQmlApplicationEngine* m_Engine;
    QLocalServer* m_Server;
    QLocalSocket* m_Socket = nullptr;
    QByteArray m_Buffer;
    bool m_Discarding = false;   // dropping the rest of a line over 1 MiB

    struct PendingLine
    {
        QPointer<QLocalSocket> socket;   // the answer goes back to this client only
        QByteArray line;
    };
    QList<PendingLine> m_Pending;
    bool m_Scheduled = false;
    bool m_Quitting = false;
    std::function<void()> m_AfterReply;
    TestProtocol::Dispatcher m_Dispatcher;

    // Injected stream mouse: position in the stream window's SDL coordinates, buttons held
    unsigned int m_MouseWindowId = 0;
    int m_MouseX = -1;
    int m_MouseY = -1;
    unsigned int m_MouseButtons = 0;

    _SDL_Joystick* m_Pad = nullptr;
    int m_PadInstance = -1;
    Session* m_PadSession = nullptr;
    QTimer* m_PadTimer;
};
