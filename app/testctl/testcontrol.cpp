#include "testcontrol.h"
#include "uiprobe.h"

#include "SDL_compat.h"
#include <SDL_syswm.h>
#include "backend/benchmark/benchmarkrunner.h"
#include "backend/clipboardmanager.h"
#include "backend/computermanager.h"
#include "backend/nvcomputer.h"
#include "backend/nvhttp.h"
#include "settings/artemissettings.h"
#include "settings/profilemanager.h"
#include "settings/streamingpreferences.h"
#include "streaming/input/testinput.h"
#include "streaming/panzoom.h"
#include "streaming/session.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaProperty>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QReadLocker>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtDebug>

#include <algorithm>
#include <cstdlib>
#include <memory>

#ifdef Q_OS_WIN32
#include <Windows.h>
#endif

using namespace TestProtocol;

// The key table (testprotocol.cpp) is written without SDL: check it against SDL here
static_assert(SDL_SCANCODE_A == 4 && SDL_SCANCODE_1 == 30 && SDL_SCANCODE_0 == 39 && SDL_SCANCODE_RETURN == 40 &&
              SDL_SCANCODE_GRAVE == 53 && SDL_SCANCODE_F1 == 58 && SDL_SCANCODE_DELETE == 76 &&
              SDL_SCANCODE_UP == 82 && SDL_SCANCODE_APPLICATION == 101 && SDL_SCANCODE_LCTRL == 224 &&
              SDL_SCANCODE_RGUI == 231, "key table scancodes are USB HID usages");
static_assert(SDLK_a == 'a' && SDLK_RETURN == '\r' && SDLK_ESCAPE == 27 && SDLK_DELETE == 127 &&
              SDLK_F1 == (58 | (1 << 30)) && SDLK_LEFT == (80 | (1 << 30)) && SDLK_LCTRL == (224 | (1 << 30)),
              "key table keycodes are SDL keycodes");

namespace {

// Files and registry key the `reset` command removes at exit (computed while the
// application object still exists)
QStringList s_WipeFolders;
QString s_WipeRegistryKey;

QStringList wipeTestOrganization()
{
    QStringList failures;
    for (const QString& folder : s_WipeFolders) {
        // Second guard, right before deleting: the folder is named after the test organisation
        if (QFileInfo(folder).fileName() != QLatin1String(kTestOrganization)) {
            failures.append(folder);
            continue;
        }
        // Never QDir::removeRecursively(): it follows junctions out of the folder
        if (!removeTreeNoFollow(folder)) {
            failures.append(folder);
        }
    }
#ifdef Q_OS_WIN32
    if (s_WipeRegistryKey == QStringLiteral("Software\\") + kTestOrganization) {
        const LSTATUS status = RegDeleteTreeW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(s_WipeRegistryKey.utf16()));
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
            failures.append("HKCU\\" + s_WipeRegistryKey);
        }
    }
#endif
    return failures;
}

void wipeAtExit()
{
    // After main() returned: settings saved while quitting are gone too
    wipeTestOrganization();
}

QJsonValue propertyToJson(const QMetaProperty& property, const QVariant& value)
{
    if (property.isEnumType()) {
        return value.toInt();
    }
    return QJsonValue::fromVariant(value);
}

struct SettingRef
{
    QObject* object = nullptr;
    QMetaProperty property;
    const char* source = nullptr;
};

// StreamingPreferences first, then ArtemisSettings: their Q_PROPERTY names are the keys
bool findSetting(const QString& key, SettingRef& out)
{
    const struct { QObject* object; const char* source; } sources[] = {
        {StreamingPreferences::get(), "streaming"},
        {ArtemisSettings::instance(), "artemis"},
    };
    for (const auto& entry : sources) {
        const QMetaObject* meta = entry.object->metaObject();
        const int index = meta->indexOfProperty(key.toUtf8().constData());
        if (index >= QObject::staticMetaObject.propertyCount()) {
            out.object = entry.object;
            out.property = meta->property(index);
            out.source = entry.source;
            return true;
        }
    }
    return false;
}

QJsonObject listSettings(QObject* object)
{
    QJsonObject values;
    const QMetaObject* meta = object->metaObject();
    for (int i = QObject::staticMetaObject.propertyCount(); i < meta->propertyCount(); i++) {
        const QMetaProperty property = meta->property(i);
        if (property.isReadable()) {
            values[property.name()] = propertyToJson(property, property.read(object));
        }
    }
    return values;
}

Uint16 sdlModifiers(int mask)
{
    Uint16 mod = 0;
    if (mask & ModCtrl) mod |= KMOD_LCTRL;
    if (mask & ModShift) mod |= KMOD_LSHIFT;
    if (mask & ModAlt) mod |= KMOD_LALT;
    if (mask & ModMeta) mod |= KMOD_LGUI;
    return mod;
}

Qt::KeyboardModifiers qtModifiers(int mask)
{
    Qt::KeyboardModifiers modifiers;
    if (mask & ModCtrl) modifiers |= Qt::ControlModifier;
    if (mask & ModShift) modifiers |= Qt::ShiftModifier;
    if (mask & ModAlt) modifiers |= Qt::AltModifier;
    if (mask & ModMeta) modifiers |= Qt::MetaModifier;
    return modifiers;
}

bool parseKeyState(const QJsonObject& args, UiProbe::KeyState& state, QString& error)
{
    const QString name = args.value("state").toString("click");
    if (name == "click") state = UiProbe::KeyState::Click;
    else if (name == "down") state = UiProbe::KeyState::Down;
    else if (name == "up") state = UiProbe::KeyState::Up;
    else {
        error = QStringLiteral("state: \"click\" (default), \"down\" or \"up\"");
        return false;
    }
    return true;
}

bool parseMouseButton(const QJsonValue& value, int& sdlButton, Qt::MouseButton& qtButton, QString& error)
{
    const QString name = value.toString("left").toLower();
    if (name == "left") { sdlButton = SDL_BUTTON_LEFT; qtButton = Qt::LeftButton; }
    else if (name == "right") { sdlButton = SDL_BUTTON_RIGHT; qtButton = Qt::RightButton; }
    else if (name == "middle") { sdlButton = SDL_BUTTON_MIDDLE; qtButton = Qt::MiddleButton; }
    else if (name == "x1") { sdlButton = SDL_BUTTON_X1; qtButton = Qt::BackButton; }
    else if (name == "x2") { sdlButton = SDL_BUTTON_X2; qtButton = Qt::ForwardButton; }
    else {
        error = QStringLiteral("button: left, right, middle, x1 or x2");
        return false;
    }
    return true;
}

QString windowRole(QQuickWindow* window, QQuickWindow* main)
{
    if (window == main) return QStringLiteral("main");
    if (window->type() == Qt::Tool) return QStringLiteral("quickmenu");
    if (window->type() == Qt::ToolTip) return QStringLiteral("toast");
    return QStringLiteral("other");
}

bool pushEvent(SDL_Event& event, QString& error)
{
    if (SDL_PushEvent(&event) != 1) {
        error = QStringLiteral("SDL_PushEvent failed: %1").arg(SDL_GetError());
        return false;
    }
    return true;
}

}

TestControl::TestControl(QQmlApplicationEngine* engine, QObject* parent)
    : QObject(parent),
      m_Engine(engine),
      m_Server(new QLocalServer(this)),
      m_PadTimer(new QTimer(this))
{
    registerCommands();
    m_PadTimer->setInterval(500);
    connect(m_PadTimer, &QTimer::timeout, this, &TestControl::checkPadSession);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &TestControl::shutdown);
}

TestControl::~TestControl()
{
    shutdown();
}

bool TestControl::start()
{
    // Only this Windows user may open the pipe
    m_Server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_Server, &QLocalServer::newConnection, this, &TestControl::onNewConnection);
    if (!m_Server->listen(QLatin1String(kServerName))) {
        qWarning() << "Test build: command channel unavailable:" << m_Server->errorString();
        return false;
    }
    qInfo() << "Test build: command channel listening on" << m_Server->fullServerName();
    return true;
}

void TestControl::shutdown()
{
    detachPad();
    detachJoystick();
    m_Pending.clear();
    if (m_Socket != nullptr) {
        m_Socket->flush();
        if (m_Socket->bytesToWrite() > 0) {
            m_Socket->waitForBytesWritten(1000);
        }
        m_Socket->disconnectFromServer();
        m_Socket = nullptr;
    }
    m_Server->close();
}

void TestControl::onNewConnection()
{
    while (m_Server->hasPendingConnections()) {
        QLocalSocket* socket = m_Server->nextPendingConnection();
        if (m_Socket != nullptr) {
            // Connected first: disconnectFromServer() may emit disconnected() right away
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            socket->write(errorLine(QJsonValue(QJsonValue::Null), QStringLiteral("another client is already connected")));
            socket->flush();
            socket->disconnectFromServer();
            continue;
        }
        m_Socket = socket;
        m_Lines.clear();
        connect(socket, &QLocalSocket::readyRead, this, &TestControl::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &TestControl::onSocketDisconnected);
    }
}

void TestControl::onSocketDisconnected()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket == m_Socket) {
        m_Socket = nullptr;
        m_Lines.clear();
        // Requests of a client that left are dropped; one that is running answers nobody
        m_Pending.erase(std::remove_if(m_Pending.begin(), m_Pending.end(),
                                       [socket](const PendingLine& pending) { return pending.socket == socket; }),
                        m_Pending.end());
    }
    if (socket != nullptr) {
        socket->deleteLater();
    }
}

void TestControl::sendLine(QLocalSocket* socket, const QByteArray& line)
{
    // Only to the client that sent the request, and only while it is still connected
    if (socket != nullptr && socket == m_Socket) {
        socket->write(line);
        socket->flush();
    }
}

void TestControl::onReadyRead()
{
    if (m_Socket == nullptr) {
        return;
    }
    // Only splits lines: commands never run here, so that none can start inside another
    // command's nested event loop (readyRead is delivered from any event loop)
    const QList<LineBuffer::Line> lines = m_Lines.feed(m_Socket->readAll());
    for (const LineBuffer::Line& line : lines) {
        if (line.tooLong) {
            sendLine(m_Socket, errorLine(QJsonValue(QJsonValue::Null), QStringLiteral("line too long")));
        }
        else {
            m_Pending.append({m_Socket, line.text});
        }
    }
    scheduleNext();
}

void TestControl::scheduleNext()
{
    if (!m_Pending.isEmpty() && !m_Scheduled) {
        m_Scheduled = true;
        QMetaObject::invokeMethod(this, &TestControl::runNext, Qt::QueuedConnection);
    }
}

void TestControl::runNext()
{
    m_Scheduled = false;
    // Delivered from a nested event loop of the running command: it waits for that command,
    // which schedules the next one when it ends
    if (m_Dispatcher.isRunning() || m_Pending.isEmpty()) {
        return;
    }

    const PendingLine pending = m_Pending.takeFirst();
    const QByteArray response = m_Dispatcher.handleLine(pending.line);
    sendLine(pending.socket, response);

    if (m_AfterReply) {
        auto action = std::move(m_AfterReply);
        m_AfterReply = nullptr;
        action();
    }
    // One command per turn of the event loop: the GUI updates between commands
    scheduleNext();
}

void TestControl::registerCommands()
{
    auto bind = [this](bool (TestControl::*method)(const QJsonObject&, QJsonObject&, QString&)) {
        return [this, method](const QJsonObject& args, QJsonObject& result, QString& error) {
            return (this->*method)(args, result, error);
        };
    };
    m_Dispatcher.add("tree", bind(&TestControl::cmdTree));
    m_Dispatcher.add("click", bind(&TestControl::cmdClick));
    m_Dispatcher.add("key", bind(&TestControl::cmdKey));
    m_Dispatcher.add("text", bind(&TestControl::cmdText));
    m_Dispatcher.add("wheel", bind(&TestControl::cmdWheel));
    m_Dispatcher.add("mouse", bind(&TestControl::cmdMouse));
    m_Dispatcher.add("gamepad", bind(&TestControl::cmdGamepad));
    m_Dispatcher.add("joystick", bind(&TestControl::cmdJoystick));
    m_Dispatcher.add("screenshot", bind(&TestControl::cmdScreenshot));
    m_Dispatcher.add("windows", bind(&TestControl::cmdWindows));
    // Resize a Qt window (index from "windows", default the main one) to w x h logical px, below its minimum size if
    // asked (a 1024 x 576 window: the dialogs must never go past its edges); never activates it. {w, h} as it is now
    m_Dispatcher.add("resize", [this](const QJsonObject& args, QJsonObject& result, QString& error) {
        QQuickWindow* window = args.contains("window") ? qtWindowArg(args, error) : mainWindow();
        if (window == nullptr) {
            if (error.isEmpty()) {
                error = QStringLiteral("no main window");
            }
            return false;
        }
        const int w = args.value("w").toInt(0);
        const int h = args.value("h").toInt(0);
        if (w < 200 || h < 200 || w > 8000 || h > 8000) {
            error = QStringLiteral("w, h: a size in logical px (200..8000)");
            return false;
        }
        window->setMinimumSize(QSize(qMin(window->minimumWidth(), w), qMin(window->minimumHeight(), h)));
        window->resize(w, h);
        result["w"] = window->width();
        result["h"] = window->height();
        return true;
    });
    // The focus rings on screen and what cuts them (UiProbe::focusRings): {rings: [...]}; "window" limits it
    m_Dispatcher.add("focusrings", [this](const QJsonObject& args, QJsonObject& result, QString& error) {
        QList<QQuickWindow*> windows = UiProbe::windows(mainWindow());
        if (args.contains("window")) {
            QQuickWindow* window = qtWindowArg(args, error);
            if (window == nullptr) {
                return false;
            }
            const int index = windows.indexOf(window);
            QJsonArray rings = UiProbe::focusRings({window});
            for (int i = 0; i < rings.size(); i++) {
                QJsonObject ring = rings[i].toObject();
                ring["window"] = index;
                rings[i] = ring;
            }
            result["rings"] = rings;
            return true;
        }
        result["rings"] = UiProbe::focusRings(windows);
        return true;
    });
    m_Dispatcher.add("state", bind(&TestControl::cmdState));
    m_Dispatcher.add("setting", bind(&TestControl::cmdSetting));
    m_Dispatcher.add("reset", bind(&TestControl::cmdReset));
    m_Dispatcher.add("quit", bind(&TestControl::cmdQuit));
    m_Dispatcher.add("clipboard", bind(&TestControl::cmdClipboard));
    m_Dispatcher.add("appasset", bind(&TestControl::cmdAppAsset));
    m_Dispatcher.add("hoststore", bind(&TestControl::cmdHostStore));
    m_Dispatcher.add("commands", [this](const QJsonObject&, QJsonObject& result, QString&) {
        result["commands"] = QJsonArray::fromStringList(m_Dispatcher.commands());
        return true;
    });
}

QQuickWindow* TestControl::mainWindow() const
{
    const QList<QObject*> roots = m_Engine->rootObjects();
    return roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
}

SDL_Window* TestControl::streamWindow() const
{
    Session* session = Session::get();
    return session != nullptr ? session->testStreamWindow() : nullptr;
}

bool TestControl::quickMenuVisible() const
{
    for (QQuickWindow* window : UiProbe::windows(mainWindow())) {
        if (window->type() == Qt::Tool) {
            return true;
        }
    }
    return false;
}

bool TestControl::targetsStream(const QJsonObject& args, QString& error) const
{
    const QString target = args.value("target").toString("auto");
    if (target == "qt") {
        return false;
    }
    if (target == "stream") {
        if (streamWindow() == nullptr) {
            error = QStringLiteral("no stream window");
        }
        return true;
    }
    if (target != "auto") {
        error = QStringLiteral("target: \"auto\" (default), \"qt\" or \"stream\"");
        return false;
    }
    return streamWindow() != nullptr && !quickMenuVisible();
}

QQuickWindow* TestControl::qtWindowArg(const QJsonObject& args, QString& error) const
{
    const QList<QQuickWindow*> windows = UiProbe::windows(mainWindow());
    if (windows.isEmpty()) {
        error = QStringLiteral("no visible Qt window");
        return nullptr;
    }
    if (!args.contains("window")) {
        return UiProbe::defaultInputWindow(windows);
    }
    const int index = args.value("window").toInt(-1);
    if (index < 0 || index >= windows.size()) {
        error = QStringLiteral("window: an index from \"windows\" (kind qt), 0..%1").arg(windows.size() - 1);
        return nullptr;
    }
    return windows[index];
}

bool TestControl::cmdTree(const QJsonObject& args, QJsonObject& result, QString& error)
{
    QList<QQuickWindow*> windows = UiProbe::windows(mainWindow());
    if (args.contains("window")) {
        QQuickWindow* window = qtWindowArg(args, error);
        if (window == nullptr) {
            return false;
        }
        windows = {window};
    }
    QJsonArray items;
    const QList<UiProbe::Item> found = UiProbe::collect(windows, args.value("textOnly").toBool(false));
    for (const UiProbe::Item& item : found) {
        QJsonObject json = UiProbe::toJson(item);
        if (args.contains("window")) {
            json["window"] = args.value("window").toInt();
        }
        items.append(json);
    }
    result["items"] = items;
    return true;
}

bool TestControl::cmdClick(const QJsonObject& args, QJsonObject& result, QString& error)
{
    int sdlButton;
    Qt::MouseButton button;
    if (!parseMouseButton(args.value("button"), sdlButton, button, error)) {
        return false;
    }

    const QList<QQuickWindow*> windows = UiProbe::windows(mainWindow());
    QQuickWindow* window = nullptr;
    QPointF pos;
    if (args.contains("x") || args.contains("y")) {
        window = qtWindowArg(args, error);
        if (window == nullptr) {
            return false;
        }
        pos = QPointF(args.value("x").toDouble(), args.value("y").toDouble());
        if (pos.x() < 0 || pos.y() < 0 || pos.x() >= window->width() || pos.y() >= window->height()) {
            error = QStringLiteral("x/y outside the window (%1x%2)").arg(window->width()).arg(window->height());
            return false;
        }
    }
    else {
        UiProbe::Item item;
        if (!UiProbe::find(UiProbe::collect(windows), args, item, error)) {
            return false;
        }
        if (QQuickItem* cover = UiProbe::coveringItem(windows, item, button)) {
            if (cover->inherits("QQuickOverlay")) {
                error = QStringLiteral("covered: a modal popup is open (the overlay takes the click)");
            }
            else {
                error = QStringLiteral("covered: a %1%2 takes the click at its centre")
                            .arg(UiProbe::typeName(cover),
                                 cover->objectName().isEmpty() ? QString() : " (" + cover->objectName() + ")");
            }
            return false;
        }
        window = windows[item.window];
        pos = item.rect.center();
        result["item"] = UiProbe::toJson(item);
    }

    UiProbe::click(window, pos, button);
    result["window"] = windows.indexOf(window);
    result["x"] = qRound(pos.x());
    result["y"] = qRound(pos.y());
    return true;
}

bool TestControl::cmdKey(const QJsonObject& args, QJsonObject& result, QString& error)
{
    const KeySpec* key = findKey(args.value("key").toString());
    if (key == nullptr) {
        error = QStringLiteral("unknown key \"%1\" (names: CLAUDE.md, \"Canal de commande\")")
                    .arg(args.value("key").toString());
        return false;
    }
    int modifiers;
    if (!parseModifiers(args.value("modifiers"), modifiers, error)) {
        return false;
    }
    UiProbe::KeyState state;
    if (!parseKeyState(args, state, error)) {
        return false;
    }

    const bool toStream = targetsStream(args, error);
    if (!error.isEmpty()) {
        return false;
    }
    if (toStream) {
        result["target"] = "stream";
        return pushKey(streamWindow(), *key, modifiers, static_cast<int>(state), error);
    }

    QQuickWindow* window = qtWindowArg(args, error);
    if (window == nullptr) {
        return false;
    }
    QQuickItem* target = UiProbe::keyTarget(window);
    result["target"] = "qt";
    result["focus"] = UiProbe::typeName(target) + (target && !target->objectName().isEmpty() ? "#" + target->objectName() : "");
    UiProbe::key(window, key->qtKey, qtModifiers(modifiers), state);
    return true;
}

bool TestControl::cmdText(const QJsonObject& args, QJsonObject& result, QString& error)
{
    if (!args.value("s").isString()) {
        error = QStringLiteral("text needs \"s\"");
        return false;
    }
    const QString text = args.value("s").toString();
    const bool toStream = targetsStream(args, error);
    if (!error.isEmpty()) {
        return false;
    }
    if (toStream) {
        result["target"] = "stream";
        return pushText(streamWindow(), text, error);
    }
    QQuickWindow* window = qtWindowArg(args, error);
    if (window == nullptr) {
        return false;
    }
    result["target"] = "qt";
    UiProbe::typeText(window, text);
    return true;
}

bool TestControl::cmdWheel(const QJsonObject& args, QJsonObject& result, QString& error)
{
    const double dx = args.value("dx").toDouble(0);
    const double dy = args.value("dy").toDouble(0);
    if (dx == 0 && dy == 0) {
        error = QStringLiteral("wheel needs \"dx\" and/or \"dy\" (notches, dy > 0 = up)");
        return false;
    }
    const bool toStream = targetsStream(args, error);
    if (!error.isEmpty()) {
        return false;
    }
    if (toStream) {
        SDL_Window* window = streamWindow();
        if (args.contains("x") || args.contains("y")) {
            if (!pushMouseMotion(window, args.value("x").toInt(), args.value("y").toInt(), error)) {
                return false;
            }
        }
        result["target"] = "stream";
        return pushWheel(window, dx, dy, error);
    }

    QQuickWindow* window = nullptr;
    QPointF pos;
    if (args.contains("text") || args.contains("objectName")) {
        const QList<QQuickWindow*> windows = UiProbe::windows(mainWindow());
        UiProbe::Item item;
        if (!UiProbe::find(UiProbe::collect(windows), args, item, error)) {
            return false;
        }
        window = windows[item.window];
        pos = item.rect.center();
    }
    else {
        window = qtWindowArg(args, error);
        if (window == nullptr) {
            return false;
        }
        pos = (args.contains("x") || args.contains("y"))
                  ? QPointF(args.value("x").toDouble(), args.value("y").toDouble())
                  : QPointF(window->width() / 2.0, window->height() / 2.0);
        if (pos.x() < 0 || pos.y() < 0 || pos.x() >= window->width() || pos.y() >= window->height()) {
            error = QStringLiteral("x/y outside the window (%1x%2)").arg(window->width()).arg(window->height());
            return false;
        }
    }
    result["target"] = "qt";
    UiProbe::wheel(window, pos, QPoint(qRound(dx * 120), qRound(dy * 120)));
    return true;
}

bool TestControl::cmdMouse(const QJsonObject& args, QJsonObject& result, QString& error)
{
    SDL_Window* window = streamWindow();
    if (window == nullptr) {
        error = QStringLiteral("mouse drives the stream window: no stream (use click/wheel for Qt windows)");
        return false;
    }
    syncMouseWindow(window);

    const QString action = args.value("action").toString();
    if (action == "move") {
        if (!args.contains("x") || !args.contains("y")) {
            error = QStringLiteral("move needs \"x\" and \"y\"");
            return false;
        }
        if (!pushMouseMotion(window, args.value("x").toInt(), args.value("y").toInt(), error)) {
            return false;
        }
    }
    else if (action == "button" || action == "click") {
        int sdlButton;
        Qt::MouseButton qtButton;
        if (!parseMouseButton(args.value("button"), sdlButton, qtButton, error)) {
            return false;
        }
        if (args.contains("x") || args.contains("y")) {
            if (!pushMouseMotion(window, args.value("x").toInt(), args.value("y").toInt(), error)) {
                return false;
            }
        }
        if (action == "click") {
            if (!pushMouseButton(window, sdlButton, true, error) || !pushMouseButton(window, sdlButton, false, error)) {
                return false;
            }
        }
        else {
            if (!args.value("down").isBool()) {
                error = QStringLiteral("button needs \"down\": true or false");
                return false;
            }
            if (!pushMouseButton(window, sdlButton, args.value("down").toBool(), error)) {
                return false;
            }
        }
    }
    else {
        error = QStringLiteral("action: \"move\", \"button\" or \"click\"");
        return false;
    }

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    result["x"] = m_MouseX;
    result["y"] = m_MouseY;
    result["buttons"] = static_cast<int>(m_MouseButtons);
    result["windowW"] = w;
    result["windowH"] = h;
    return true;
}

bool TestControl::cmdGamepad(const QJsonObject& args, QJsonObject& result, QString& error)
{
    if (args.value("detach").toBool(false)) {
        result["detached"] = m_Pad != nullptr;
        detachPad();
        return true;
    }

    if (args.value("attach").toBool(false)) {
        const bool wasAttached = m_Pad != nullptr;
        if (!attachPad(error)) {
            return false;
        }
        result["pad"] = kTestGamepadName;
        result["instance"] = m_PadInstance;
        result["attached"] = !wasAttached;
        return true;
    }
    // A press sent before the client has opened the pad would be lost: attaching is its own step
    if (m_Pad == nullptr) {
        error = QStringLiteral("no pad: send {\"cmd\":\"gamepad\",\"attach\":true} first, then give the client "
                               "about 0.5 s to open it");
        return false;
    }
    result["pad"] = kTestGamepadName;
    result["instance"] = m_PadInstance;

    if (args.contains("button")) {
        const QString name = args.value("button").toString();
        const SDL_GameControllerButton button = SDL_GameControllerGetButtonFromString(name.toUtf8().constData());
        // The pad has the 15 buttons of a standard controller (A ... D-pad right)
        if (button == SDL_CONTROLLER_BUTTON_INVALID || button > SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            error = QStringLiteral("button: a, b, x, y, back, guide, start, leftstick, rightstick, "
                                   "leftshoulder, rightshoulder, dpup, dpdown, dpleft, dpright");
            return false;
        }
        const QJsonValue value = args.value("value");
        const bool pressed = value.isBool() ? value.toBool() : value.toDouble(0) != 0;
        if (SDL_JoystickSetVirtualButton(m_Pad, button, pressed ? SDL_PRESSED : SDL_RELEASED) != 0) {
            error = QStringLiteral("SDL_JoystickSetVirtualButton failed: %1").arg(SDL_GetError());
            return false;
        }
        result["button"] = name;
        result["value"] = pressed;
        return true;
    }
    if (args.contains("axis")) {
        const QString name = args.value("axis").toString();
        const SDL_GameControllerAxis axis = SDL_GameControllerGetAxisFromString(name.toUtf8().constData());
        if (axis == SDL_CONTROLLER_AXIS_INVALID) {
            error = QStringLiteral("axis: leftx, lefty, rightx, righty, lefttrigger, righttrigger");
            return false;
        }
        const int value = qBound(-32768, args.value("value").toInt(0), 32767);
        if (SDL_JoystickSetVirtualAxis(m_Pad, axis, static_cast<Sint16>(value)) != 0) {
            error = QStringLiteral("SDL_JoystickSetVirtualAxis failed: %1").arg(SDL_GetError());
            return false;
        }
        result["axis"] = name;
        result["value"] = value;
        return true;
    }
    error = QStringLiteral("gamepad needs \"button\" or \"axis\" with \"value\" (or \"attach\" / \"detach\")");
    return false;
}

bool TestControl::cmdJoystick(const QJsonObject& args, QJsonObject& result, QString& error)
{
    if (args.value("detach").toBool(false)) {
        result["detached"] = m_Joystick != nullptr;
        detachJoystick();
        return true;
    }
    if (!args.value("attach").toBool(false)) {
        error = QStringLiteral("joystick needs {\"attach\": true, \"kind\": \"generic\", \"axes\", \"buttons\", \"hats\"} "
                               "or {\"detach\": true}");
        return false;
    }
    if (m_Joystick != nullptr) {
        error = QStringLiteral("a joystick is already attached (detach it first)");
        return false;
    }
    if (args.value("kind").toString(QStringLiteral("generic")) != QLatin1String("generic")) {
        error = QStringLiteral("kind: \"generic\" (a HID joystick SDL has no mapping for)");
        return false;
    }
    const int axes = args.value("axes").toInt(4);
    const int buttons = args.value("buttons").toInt(8);
    const int hats = args.value("hats").toInt(1);
    if (axes < 0 || axes > 16 || buttons < 0 || buttons > 32 || hats < 0 || hats > 4) {
        error = QStringLiteral("axes 0-16, buttons 0-32, hats 0-4");
        return false;
    }

    // Our own reference to the subsystem, like the pad. Usable outside a stream (the mapper, the
    // guessed-layout warning at launch); the stream never opens it (testinput.h: only the pad).
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
        error = QStringLiteral("SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed: %1").arg(SDL_GetError());
        return false;
    }
    // Type unknown, no VID/PID: SDL has no mapping for it, like a generic HID joystick
    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_UNKNOWN;
    desc.naxes = static_cast<Uint16>(axes);
    desc.nbuttons = static_cast<Uint16>(buttons);
    desc.nhats = static_cast<Uint16>(hats);
    desc.name = kTestJoystickName;
    const int index = SDL_JoystickAttachVirtualEx(&desc);
    if (index < 0) {
        error = QStringLiteral("SDL_JoystickAttachVirtualEx failed: %1").arg(SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        return false;
    }
    m_Joystick = SDL_JoystickOpen(index);
    if (m_Joystick == nullptr) {
        error = QStringLiteral("SDL_JoystickOpen failed: %1").arg(SDL_GetError());
        SDL_JoystickDetachVirtual(index);
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        return false;
    }
    m_JoystickInstance = SDL_JoystickInstanceID(m_Joystick);
    char guid[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(m_Joystick), guid, sizeof(guid));
    result["name"] = kTestJoystickName;
    result["guid"] = QString::fromLatin1(guid);
    result["instance"] = m_JoystickInstance;
    result["gameController"] = SDL_IsGameController(index) == SDL_TRUE;
    qInfo() << "Test build: generic joystick attached, instance" << m_JoystickInstance << guid;
    return true;
}

void TestControl::detachJoystick()
{
    if (m_Joystick == nullptr) {
        return;
    }
    SDL_JoystickClose(m_Joystick);
    m_Joystick = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_JoystickGetDeviceInstanceID(i) == m_JoystickInstance) {
            SDL_JoystickDetachVirtual(i);
            break;
        }
    }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    m_JoystickInstance = -1;
    qInfo() << "Test build: generic joystick detached";
}

bool TestControl::cmdScreenshot(const QJsonObject& args, QJsonObject& result, QString& error)
{
    const QString path = args.value("path").toString();
    if (!isLocalAbsolutePath(path)) {
        error = QStringLiteral("path must be absolute, on a local drive (no network path)");
        return false;
    }
    QQuickWindow* window = qtWindowArg(args.contains("window") ? args : QJsonObject{{"window", 0}}, error);
    if (window == nullptr) {
        return false;
    }
    const QImage image = window->grabWindow();
    if (image.isNull()) {
        error = QStringLiteral("grabWindow failed");
        return false;
    }
    if (!image.save(path, "PNG")) {
        error = QStringLiteral("cannot write %1").arg(path);
        return false;
    }
    result["path"] = path;
    result["width"] = image.width();
    result["height"] = image.height();
    result["dpr"] = window->devicePixelRatio();
    return true;
}

bool TestControl::cmdWindows(const QJsonObject&, QJsonObject& result, QString&)
{
    QQuickWindow* main = mainWindow();
    const QList<QQuickWindow*> windows = UiProbe::windows(main);
    QJsonArray list;
    for (int i = 0; i < windows.size(); i++) {
        QQuickWindow* window = windows[i];
        const QRect geometry = window->geometry();
        list.append(QJsonObject{
            {"index", i},
            {"kind", "qt"},
            {"role", windowRole(window, main)},
            {"title", window->title()},
            {"hwnd", static_cast<double>(window->winId())},
            {"x", geometry.x()}, {"y", geometry.y()}, {"w", geometry.width()}, {"h", geometry.height()},
            {"dpr", window->devicePixelRatio()},
        });
    }
    if (SDL_Window* stream = streamWindow()) {
        QJsonObject entry{{"kind", "stream"}, {"title", QString::fromUtf8(SDL_GetWindowTitle(stream))}};
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (SDL_GetWindowWMInfo(stream, &info)) {
#ifdef Q_OS_WIN32
            if (info.subsystem == SDL_SYSWM_WINDOWS) {
                entry["hwnd"] = static_cast<double>(reinterpret_cast<quintptr>(info.info.win.window));
            }
#endif
        }
        int x, y, w, h;
        SDL_GetWindowPosition(stream, &x, &y);
        SDL_GetWindowSize(stream, &w, &h);
        entry["x"] = x;
        entry["y"] = y;
        entry["w"] = w;
        entry["h"] = h;
        list.append(entry);
    }
    result["windows"] = list;
    return true;
}

bool TestControl::cmdState(const QJsonObject&, QJsonObject& result, QString& error)
{
    auto* manager = m_Engine->singletonInstance<ComputerManager*>("ComputerManager", "ComputerManager");
    if (manager == nullptr) {
        error = QStringLiteral("ComputerManager singleton unavailable");
        return false;
    }

    QString pageTitle;
    bool pageBusy = false;
    result["page"] = UiProbe::currentPage(mainWindow(), &pageTitle, &pageBusy);
    result["pageTitle"] = pageTitle;
    result["pageBusy"] = pageBusy;
    result["quickMenu"] = quickMenuVisible();
    result["pad"] = m_Pad != nullptr;

    QJsonArray computers;
    {
        const QVector<NvComputer*> hosts = manager->getComputers();
        for (NvComputer* computer : hosts) {
            QReadLocker lock(&computer->lock);
            QJsonArray apps;
            // The same list with what LavApollo says of each app (Playnite games: installed, last
            // activity in Unix seconds, play time in seconds, background), in the client's order
            QJsonArray appDetails;
            for (const NvApp& app : computer->appList) {
                apps.append(app.name);
                appDetails.append(QJsonObject{
                    {"name", app.name},
                    {"id", app.id},
                    {"playniteGame", app.isPlayniteGame},
                    {"installed", app.playniteInstalled},
                    {"lastActivity", app.playniteLastActivity},
                    {"playtime", app.playnitePlaytime},
                    {"hasBackground", app.playniteHasBackground},
                });
            }
            computers.append(QJsonObject{
                {"name", computer->name},
                {"uuid", computer->uuid},
                {"address", computer->activeAddress.toString()},
                {"online", computer->state == NvComputer::CS_ONLINE},
                {"state", computer->state == NvComputer::CS_ONLINE ? "online"
                          : computer->state == NvComputer::CS_OFFLINE ? "offline" : "unknown"},
                {"paired", computer->pairState == NvComputer::PS_PAIRED},
                {"testHost", computer->isTestHost},
                {"thisPc", computer->isThisPc},
                {"currentGameId", computer->currentGameId},
                {"benchmarkAppId", computer->benchmarkAppId},
                {"apps", apps},
                {"appDetails", appDetails},
                // Prototype data (wave 5): kind of PC, link, round trip, session and Playnite game starts
                // (ms since the epoch, this PC's clock; 0 = unknown)
                {"formFactor", computer->hostFormFactor},
                {"link", computer->hostLink},
                {"latencyMs", computer->latencyMs},
                {"sessionStartedAt", computer->currentGameStartedAt},
                {"playniteGameName", computer->playniteGameName},
                {"playniteGameStartedAt", computer->playniteGameStartedAt},
            });
        }
    }
    result["computers"] = computers;

    // Réglages › Profil: the profiles and the one in use ("" = the global settings)
    ProfileManager* profiles = ProfileManager::instance();
    QJsonArray profileList;
    for (const QVariant& entry : profiles->profileList()) {
        const QVariantMap profile = entry.toMap();
        profileList.append(QJsonObject{
            {"id", profile.value("id").toString()},
            {"name", profile.value("name").toString()},
            {"active", profile.value("active").toBool()},
        });
    }
    result["profiles"] = profileList;
    result["activeProfileId"] = profiles->activeProfileId();

    QJsonObject session;
    Session* active = Session::get();
    session["active"] = active != nullptr;
    session["streaming"] = streamWindow() != nullptr;
    Benchmark::RawStats stats;
    const bool hasStats = active != nullptr && active->benchmarkSnapshot(stats);
    session["frames"] = hasStats ? static_cast<double>(stats.framesTotal) : 0;
    session["received"] = hasStats ? static_cast<double>(stats.framesReceived) : 0;
    session["decoded"] = hasStats ? static_cast<double>(stats.framesDecoded) : 0;
    session["rendered"] = hasStats ? static_cast<double>(stats.framesRendered) : 0;
    session["dropped"] = hasStats ? static_cast<double>(stats.framesNetworkLost + stats.framesPacerDropped) : 0;
    // The negotiated stream (what the host encodes), not the settings: HostDisplayFit may have
    // changed it. 0 until the stream's SDL loop runs, when the configuration is final.
    const bool streaming = streamWindow() != nullptr;
    session["width"] = streaming ? active->testStreamConfig().width : 0;
    session["height"] = streaming ? active->testStreamConfig().height : 0;
    session["fps"] = streaming ? active->testStreamConfig().fps : 0;
    // Reconnections after a host display change, since the process started
    session["restarts"] = Session::testRestartCount();
    // What the stream window shows over the video: the performance overlay and the status messages
    // (slow connection, gamepad mouse mode, muted microphone, notifications...)
    if (active != nullptr) {
        Overlay::OverlayManager& overlay = active->getOverlayManager();
        const bool statsShown = overlay.isOverlayEnabled(Overlay::OverlayDebug);
        session["overlayStats"] = QJsonObject{
            {"shown", statsShown},
            {"text", statsShown ? QString::fromStdString(overlay.getOverlayText(Overlay::OverlayDebug)) : QString()},
        };
        session["statusMessage"] = overlay.isOverlayEnabled(Overlay::OverlayStatusUpdate)
                ? QString::fromStdString(overlay.getOverlayText(Overlay::OverlayStatusUpdate)) : QString();
        session["micActive"] = active->isMicrophoneActive();
        session["micMuted"] = active->isMicrophoneMuted();
        session["launchWarnings"] = QJsonArray::fromStringList(active->property("launchWarnings").toStringList());
    }
    else {
        session["overlayStats"] = QJsonObject{{"shown", false}, {"text", QString()}};
        session["statusMessage"] = QString();
        session["micActive"] = false;
        session["micMuted"] = false;
        session["launchWarnings"] = QJsonArray();
    }
    session["zoom"] = PanZoom::scale();
    result["session"] = session;
    // In-game notifications shown since the process started, oldest first; "toast" = the last one
    const QStringList& toasts = Session::testToasts();
    result["toasts"] = QJsonArray::fromStringList(toasts);
    result["toast"] = toasts.isEmpty() ? QString() : toasts.last();

    // Benchmark des réglages: the runner and its report as saved (docs/benchmark.md)
    QJsonObject benchmark;
    if (auto* runner = m_Engine->singletonInstance<BenchmarkRunner*>("BenchmarkRunner", "BenchmarkRunner")) {
        benchmark["running"] = runner->running();
        benchmark["message"] = runner->message();
        benchmark["report"] = runner->testReportJson();
        benchmark["historyDir"] = Benchmark::History::defaultDirectory();
    }
    result["benchmark"] = benchmark;
    return true;
}

bool TestControl::cmdAppAsset(const QJsonObject& args, QJsonObject& result, QString& error)
{
    // Fetches an app's image the way the client does (NvHTTP::getBoxArt / getBackground), to show
    // what the host serves: {"host": name, "appId": id, "kind": "cover" | "background"} ->
    // {"width", "height"}, 0 x 0 when the host has none
    auto* manager = m_Engine->singletonInstance<ComputerManager*>("ComputerManager", "ComputerManager");
    if (manager == nullptr) {
        error = QStringLiteral("ComputerManager singleton unavailable");
        return false;
    }
    const QString hostName = args.value("host").toString();
    const int appId = args.value("appId").toInt();
    const QString kind = args.value("kind").toString("cover");
    if (kind != "cover" && kind != "background") {
        error = QStringLiteral("kind: \"cover\" (default) or \"background\"");
        return false;
    }
    NvComputer* target = nullptr;
    for (NvComputer* computer : manager->getComputers()) {
        QReadLocker lock(&computer->lock);
        if (computer->name == hostName) {
            target = computer;
            break;
        }
    }
    if (target == nullptr) {
        error = QStringLiteral("no host named ") + hostName;
        return false;
    }
    QImage image;
    try {
        NvHTTP http(target);
        image = kind == "background" ? http.getBackground(appId) : http.getBoxArt(appId);
    } catch (...) {
        // 404 and other failures: no image
    }
    result["width"] = image.isNull() ? 0 : image.width();
    result["height"] = image.isNull() ? 0 : image.height();
    return true;
}

namespace {

// The stored host list, one map of keys per host, read as ComputerManager's constructor reads it:
// "hostsbackup" while it exists, else "hosts". A flush writes the backup, removes and rewrites
// "hosts", then removes the backup: reading "hosts" alone in between found it empty (a stored PC
// "missing" right after the list showed it)
QList<QVariantMap> readStoredHosts()
{
    QList<QVariantMap> hosts;
    QSettings settings;
    int count = settings.beginReadArray(QStringLiteral("hostsbackup"));
    if (count == 0) {
        settings.endArray();
        count = settings.beginReadArray(QStringLiteral("hosts"));
    }
    for (int i = 0; i < count; i++) {
        settings.setArrayIndex(i);
        QVariantMap entry;
        const QStringList keys = settings.allKeys();
        for (const QString& key : keys) {
            entry.insert(key, settings.value(key));
        }
        hosts.append(entry);
    }
    settings.endArray();
    return hosts;
}

}

bool TestControl::cmdHostStore(const QJsonObject& args, QJsonObject& result, QString& error)
{
    // The host list as another LavArtemis process sees it on disk (there is no single instance: an
    // art:// pairing link starts a second process, which stores the hosts it pairs). Only two
    // processes can show what happens between them, and the bench runs one client at a time, so
    // this command plays the other one:
    //   {"action": "list"}             -> {"hosts": [{"uuid", "name"}]}, as stored right now
    //   {"action": "keep", "uuid": u}  -> remembers that host's stored entry, keys as written
    //   {"action": "restore"}          -> writes it back into the stored list (replacing an entry
    //                                     of the same uuid), as the other process's flush would
    const QString action = args.value("action").toString("list");
    if (action == "list") {
        QJsonArray hosts;
        for (const QVariantMap& entry : readStoredHosts()) {
            QJsonObject h;
            h["uuid"] = entry.value("uuid").toString();
            h["name"] = entry.value("hostname").toString();
            hosts.append(h);
        }
        result["hosts"] = hosts;
        return true;
    }
    if (action == "keep") {
        const QString uuid = args.value("uuid").toString();
        for (const QVariantMap& entry : readStoredHosts()) {
            if (!uuid.isEmpty() && entry.value("uuid").toString() == uuid) {
                m_KeptHostEntry = entry;
                result["kept"] = uuid;
                return true;
            }
        }
        error = QStringLiteral("no stored host with uuid ") + uuid;
        return false;
    }
    if (action == "restore") {
        if (m_KeptHostEntry.isEmpty()) {
            error = QStringLiteral("nothing kept (\"keep\" first)");
            return false;
        }
        const QString uuid = m_KeptHostEntry.value("uuid").toString();
        QList<QVariantMap> hosts = readStoredHosts();
        hosts.erase(std::remove_if(hosts.begin(), hosts.end(), [&uuid](const QVariantMap& entry) {
            return entry.value("uuid").toString() == uuid;
        }), hosts.end());
        hosts.append(m_KeptHostEntry);
        QSettings settings;
        settings.remove(QStringLiteral("hosts"));
        settings.beginWriteArray(QStringLiteral("hosts"));
        for (int i = 0; i < hosts.count(); i++) {
            settings.setArrayIndex(i);
            for (auto it = hosts[i].constBegin(); it != hosts[i].constEnd(); ++it) {
                settings.setValue(it.key(), it.value());
            }
        }
        settings.endArray();
        settings.sync();
        result["restored"] = uuid;
        result["count"] = static_cast<int>(hosts.count());
        return true;
    }
    error = QStringLiteral("action: \"list\" (default), \"keep\" with \"uuid\", or \"restore\"");
    return false;
}

bool TestControl::cmdClipboard(const QJsonObject& args, QJsonObject& result, QString& error)
{
    // This PC's clipboard as the client sees it: in the test build, an in-process text, never the
    // system clipboard (ClipboardManager::testClipboardText)
    const QString action = args.value("action").toString("get");
    if (action == "set") {
        if (!args.value("text").isString()) {
            error = QStringLiteral("set needs \"text\"");
            return false;
        }
        ClipboardManager::setTestClipboardText(args.value("text").toString());
    }
    else if (action != "get") {
        error = QStringLiteral("action: \"get\" (default) or \"set\"");
        return false;
    }
    result["text"] = ClipboardManager::testClipboardText();
    return true;
}

bool TestControl::cmdSetting(const QJsonObject& args, QJsonObject& result, QString& error)
{
    const QString action = args.value("action").toString();
    if (action == "list") {
        result["streaming"] = listSettings(StreamingPreferences::get());
        result["artemis"] = listSettings(ArtemisSettings::instance());
        return true;
    }

    const QString key = args.value("key").toString();
    if (action == "raw") {
        // What is stored under a storage key (StreamingPreferences' QSettings names: "bitrate",
        // "autoadjustbitrate", "fps"...), with no fall-through: the global value, each profile's,
        // and artemis-settings.ini's (ArtemisSettings keys)
        if (key.isEmpty()) {
            error = QStringLiteral("raw needs \"key\" (a storage key, e.g. \"bitrate\")");
            return false;
        }
        const QVariantMap raw = ProfileManager::instance()->testRawValue(key);
        result["key"] = key;
        result["globalStored"] = raw.value("globalStored").toBool();
        result["global"] = QJsonValue::fromVariant(raw.value("global"));
        result["profiles"] = QJsonArray::fromVariantList(raw.value("profiles").toList());
        QSettings artemis(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
                              QStringLiteral("/artemis-settings.ini"),
                          QSettings::IniFormat);
        result["artemisStored"] = artemis.contains(key);
        result["artemis"] = QJsonValue::fromVariant(artemis.value(key));
        return true;
    }
    SettingRef setting;
    if (!findSetting(key, setting)) {
        error = QStringLiteral("unknown setting \"%1\" (\"setting\" with \"action\": \"list\" lists them)").arg(key);
        return false;
    }
    result["key"] = key;
    result["source"] = setting.source;

    if (action == "set" || action == "reset") {
        if (!setting.property.isWritable()) {
            error = QStringLiteral("\"%1\" is read-only").arg(key);
            return false;
        }

        QVariant value;
        if (action == "set") {
            if (!args.contains("value")) {
                error = QStringLiteral("set needs \"value\"");
                return false;
            }
            const QJsonValue json = args.value("value");
            if (setting.property.isEnumType() && json.isString()) {
                bool ok = false;
                const int number = setting.property.enumerator().keyToValue(json.toString().toUtf8().constData(), &ok);
                if (!ok) {
                    error = QStringLiteral("\"%1\" is not a value of %2").arg(json.toString(), setting.property.typeName());
                    return false;
                }
                value = number;
            }
            else if (setting.property.isEnumType()) {
                value = json.toInt();
            }
            else {
                value = json.toVariant();
            }
        }
        else {
            // The value of a fresh install: an object that reads nothing from storage
            QObject* defaults = nullptr;
            if (setting.object == StreamingPreferences::get()) {
                ProfileManager::setReadDefaultsForTest(true);
                defaults = StreamingPreferences::createDetached();
                ProfileManager::setReadDefaultsForTest(false);
            }
            else {
                defaults = ArtemisSettings::createDefaultsForTest();
            }
            value = setting.property.read(defaults);
            delete defaults;
        }

        if (!setting.property.write(setting.object, value)) {
            error = QStringLiteral("cannot set \"%1\" to %2").arg(key, value.toString());
            return false;
        }
        // Persisted like Réglages does (SettingsView2.saveNow): ArtemisSettings setters do not save
        StreamingPreferences::get()->save();
        ArtemisSettings::instance()->save();
    }
    else if (action != "get") {
        error = QStringLiteral("action: \"get\", \"set\", \"reset\", \"list\" or \"raw\"");
        return false;
    }

    result["value"] = propertyToJson(setting.property, setting.property.read(setting.object));
    return true;
}

bool TestControl::cmdReset(const QJsonObject&, QJsonObject& result, QString& error)
{
    const QString organization = QCoreApplication::organizationName();
    if (!isWipeableOrganization(organization)) {
        error = QStringLiteral("refused: organisation \"%1\" is not \"%2\"").arg(organization, kTestOrganization);
        return false;
    }

    QStringList folders;
    for (QStandardPaths::StandardLocation location :
         {QStandardPaths::AppConfigLocation, QStandardPaths::AppDataLocation, QStandardPaths::AppLocalDataLocation,
          QStandardPaths::CacheLocation}) {
        const QString folder = organizationFolder(QStandardPaths::writableLocation(location), organization);
        if (!folder.isEmpty() && !folders.contains(folder)) {
            folders.append(folder);
        }
    }
    s_WipeFolders = folders;
    s_WipeRegistryKey = registryKeyFor(organization);

    // Now, and again once the process is done with its settings (quitting saves some)
    const QStringList failures = wipeTestOrganization();
    static bool registered = false;
    if (!registered) {
        registered = std::atexit(wipeAtExit) == 0;
    }

    result["folders"] = QJsonArray::fromStringList(folders);
#ifdef Q_OS_WIN32
    result["registry"] = "HKCU\\" + s_WipeRegistryKey;
#endif
    result["notYetRemoved"] = QJsonArray::fromStringList(failures);
    result["quitting"] = true;
    m_AfterReply = [this]() { beginQuit(); };
    return true;
}

bool TestControl::cmdQuit(const QJsonObject&, QJsonObject& result, QString&)
{
    result["quitting"] = true;
    result["session"] = Session::get() != nullptr;
    m_AfterReply = [this]() { beginQuit(); };
    return true;
}

void TestControl::beginQuit()
{
    if (m_Quitting) {
        return;
    }
    m_Quitting = true;

    // A session ends first, as when the user leaves it: its cleanup quits the host app if
    // the settings say so, and must not race the application's exit
    if (Session* session = Session::get()) {
        session->interrupt();
    }
    auto* timer = new QTimer(this);
    auto* clock = new QElapsedTimer();
    clock->start();
    timer->setInterval(100);
    connect(timer, &QTimer::timeout, this, [this, timer, clock]() {
        if (Session::get() != nullptr && clock->elapsed() < 30000) {
            return;
        }
        timer->stop();
        delete clock;
        detachPad();
        detachJoystick();
        QCoreApplication::quit();
    });
    timer->start();
}

void TestControl::syncMouseWindow(SDL_Window* window)
{
    const Uint32 id = SDL_GetWindowID(window);
    if (id != m_MouseWindowId) {
        // A new stream window: the injected mouse starts in its centre, no button held
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        m_MouseWindowId = id;
        m_MouseX = w / 2;
        m_MouseY = h / 2;
        m_MouseButtons = 0;
    }
}

bool TestControl::pushKey(SDL_Window* window, const KeySpec& key, int modifiers, int state, QString& error)
{
    const auto keyState = static_cast<UiProbe::KeyState>(state);
    const Uint32 windowId = SDL_GetWindowID(window);
    Uint16 mod = 0;
    auto push = [&](const KeySpec& spec, bool down) {
        SDL_Event event;
        SDL_zero(event);
        event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.timestamp = SDL_GetTicks();
        event.key.windowID = windowId;
        event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
        event.key.repeat = 0;
        event.key.keysym.scancode = static_cast<SDL_Scancode>(spec.scancode);
        event.key.keysym.sym = spec.sdlKey;
        event.key.keysym.mod = mod;
        return pushEvent(event, error);
    };

    // A real keyboard: modifiers down, the key, then modifiers up in reverse order
    const struct { int flag; const char* key; Uint16 mod; } kModifierKeys[] = {
        {ModCtrl, "LCtrl", KMOD_LCTRL}, {ModShift, "LShift", KMOD_LSHIFT},
        {ModAlt, "LAlt", KMOD_LALT}, {ModMeta, "LMeta", KMOD_LGUI},
    };
    if (keyState != UiProbe::KeyState::Up) {
        for (const auto& modifier : kModifierKeys) {
            if (modifiers & modifier.flag) {
                mod |= modifier.mod;
                if (!push(*findKey(modifier.key), true)) return false;
            }
        }
        if (!push(key, true)) return false;
    }
    if (keyState != UiProbe::KeyState::Down) {
        mod = sdlModifiers(modifiers);
        if (!push(key, false)) return false;
        for (int i = 3; i >= 0; i--) {
            if (modifiers & kModifierKeys[i].flag) {
                mod &= ~kModifierKeys[i].mod;
                if (!push(*findKey(kModifierKeys[i].key), false)) return false;
            }
        }
    }
    return true;
}

bool TestControl::pushText(SDL_Window* window, const QString& text, QString& error)
{
    // The stream takes keys, not text (sdl2-compat crashes on a pushed SDL_TEXTINPUT, and the
    // client sends key codes anyway): each character is its key on a US keyboard
    QList<QPair<KeySpec, bool>> keys;
    for (const QChar c : text) {
        KeySpec key;
        bool shift;
        if (!keyForChar(c, key, shift)) {
            error = QStringLiteral("character U+%1 has no key on a US keyboard").arg(int(c.unicode()), 4, 16, QChar('0'));
            return false;
        }
        keys.append({key, shift});
    }
    for (const auto& entry : keys) {
        if (!pushKey(window, entry.first, entry.second ? ModShift : 0, static_cast<int>(UiProbe::KeyState::Click), error)) {
            return false;
        }
    }
    return true;
}

bool TestControl::pushMouseMotion(SDL_Window* window, int x, int y, QString& error)
{
    syncMouseWindow(window);
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEMOTION;
    event.motion.timestamp = SDL_GetTicks();
    event.motion.windowID = SDL_GetWindowID(window);
    event.motion.which = kTestInjectedMouseId;    // the only mouse the test build forwards
    event.motion.state = m_MouseButtons;           // absolute mode reads "buttons still down" from it
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = x - m_MouseX;
    event.motion.yrel = y - m_MouseY;
    if (!pushEvent(event, error)) {
        return false;
    }
    m_MouseX = x;
    m_MouseY = y;
    return true;
}

bool TestControl::pushMouseButton(SDL_Window* window, int button, bool down, QString& error)
{
    syncMouseWindow(window);
    SDL_Event event;
    SDL_zero(event);
    event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.timestamp = SDL_GetTicks();
    event.button.windowID = SDL_GetWindowID(window);
    event.button.which = kTestInjectedMouseId;
    event.button.button = static_cast<Uint8>(button);
    event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = m_MouseX;
    event.button.y = m_MouseY;
    if (!pushEvent(event, error)) {
        return false;
    }
    if (down) {
        m_MouseButtons |= SDL_BUTTON(button);
    }
    else {
        m_MouseButtons &= ~SDL_BUTTON(button);
    }
    return true;
}

bool TestControl::pushWheel(SDL_Window* window, double dx, double dy, QString& error)
{
    syncMouseWindow(window);
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEWHEEL;
    event.wheel.timestamp = SDL_GetTicks();
    event.wheel.windowID = SDL_GetWindowID(window);
    event.wheel.which = kTestInjectedMouseId;
    // The client reads preciseX/Y. sdl2-compat returns a wrong integer y for pushed wheel
    // events (seen: y = mouseY), the float values come through intact.
    event.wheel.x = static_cast<Sint32>(dx);
    event.wheel.y = static_cast<Sint32>(dy);
    event.wheel.preciseX = static_cast<float>(dx);
    event.wheel.preciseY = static_cast<float>(dy);
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouseX = m_MouseX;  // absolute mode ignores a wheel outside the video
    event.wheel.mouseY = m_MouseY;
    return pushEvent(event, error);
}

bool TestControl::attachPad(QString& error)
{
    if (m_Pad != nullptr) {
        return true;
    }
    if (streamWindow() == nullptr) {
        error = QStringLiteral("gamepad: no stream (the pad lives for one stream)");
        return false;
    }

    // Our own reference: the pad must outlive the session's input handler, which quits the
    // joystick subsystem first (detaching every virtual joystick with it)
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
        error = QStringLiteral("SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed: %1").arg(SDL_GetError());
        return false;
    }

    // A standard controller by its name only: no known VID/PID, whose mapping would rename it.
    // SDL makes it a game controller itself (type GAMECONTROLLER: automatic mapping a:b0 ...
    // dpright:b14, leftx:a0 ... righttrigger:a5).
    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_DPAD_RIGHT + 1;
    desc.name = kTestGamepadName;
    const int index = SDL_JoystickAttachVirtualEx(&desc);
    if (index < 0) {
        error = QStringLiteral("SDL_JoystickAttachVirtualEx failed: %1").arg(SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        return false;
    }
    m_Pad = SDL_JoystickOpen(index);
    if (m_Pad == nullptr) {
        error = QStringLiteral("SDL_JoystickOpen failed: %1").arg(SDL_GetError());
        SDL_JoystickDetachVirtual(index);
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        return false;
    }
    m_PadInstance = SDL_JoystickInstanceID(m_Pad);
    m_PadSession = Session::get();
    m_PadTimer->start();
    qInfo() << "Test build: virtual gamepad attached, instance" << m_PadInstance;
    return true;
}

void TestControl::detachPad()
{
    if (m_Pad == nullptr) {
        return;
    }
    m_PadTimer->stop();
    SDL_JoystickClose(m_Pad);
    m_Pad = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_JoystickGetDeviceInstanceID(i) == m_PadInstance) {
            SDL_JoystickDetachVirtual(i);
            break;
        }
    }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    m_PadInstance = -1;
    m_PadSession = nullptr;
    qInfo() << "Test build: virtual gamepad detached";
}

void TestControl::checkPadSession()
{
    // The pad lives for one stream
    if (Session::get() != m_PadSession || streamWindow() == nullptr) {
        detachPad();
    }
}
