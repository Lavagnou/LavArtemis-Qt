#pragma once

// Test build only (CONFIG+=lav_test): the testbench command channel's protocol, without SDL,
// a session or Qt Quick, so that tests/testguard can check it on its own.
//
// One JSON object per line in: {"id": n, "cmd": "...", ...arguments}; one per line out:
// {"id": n, "ok": true, ...result} or {"id": n, "ok": false, "error": "..."}. A line that is
// not a JSON object with a string "cmd" gets ok:false with id null. See CLAUDE.md, "Canal de
// commande", for the commands.

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace TestProtocol {

struct Request
{
    QJsonValue id;      // echoed as is (null when absent)
    QString cmd;
    QJsonObject args;   // the whole request object
};

// False with an error for a line that is not a JSON object with a non-empty string "cmd".
bool parseRequest(const QByteArray& line, Request& out, QString& error);

// Response lines, '\n'-terminated. The result's keys are merged into the response; "id" and
// "ok" are always the protocol's own.
QByteArray okLine(const QJsonValue& id, const QJsonObject& result);
QByteArray errorLine(const QJsonValue& id, const QString& error);

class Dispatcher
{
public:
    // Fills result (or error) and returns whether the command succeeded
    using Handler = std::function<bool(const QJsonObject& args, QJsonObject& result, QString& error)>;

    void add(const QString& cmd, Handler handler);

    // One request line in, one response line out
    QByteArray handleLine(const QByteArray& line);

    QStringList commands() const;

    // True while a command runs. handleLine() refuses to start a command then: a handler that
    // runs a nested event loop must never see another command execute inside it.
    bool isRunning() const { return m_Running; }

private:
    QHash<QString, Handler> m_Handlers;
    bool m_Running = false;
};

// Splits the byte stream of one client into request lines. A line longer than kMaxLine bytes
// (newline excluded) is never returned: it gives one entry with tooLong set, whether it came in
// one read or in pieces, and the rest of it is dropped up to its newline. Lines are trimmed
// (so "\r\n" works); empty lines are skipped.
class LineBuffer
{
public:
    static constexpr int kMaxLine = 1 << 20;

    struct Line
    {
        QByteArray text;
        bool tooLong = false;
    };

    QList<Line> feed(const QByteArray& data);
    void clear();

private:
    QByteArray m_Buffer;
    bool m_Discarding = false;
};

// A key by name. Values are plain ints so that this file needs neither Qt GUI nor SDL:
// qtKey is a Qt::Key, scancode a USB HID usage (= SDL_Scancode), sdlKey the SDL keycode of
// that key on a US layout (= SDL_Keycode). testcontrol.cpp checks them against SDL.
struct KeySpec
{
    const char* name;
    int qtKey;
    int scancode;
    int sdlKey;
};

// Accepted names (case-insensitive): "A".."Z", "0".."9", "F1".."F12", "Enter" ("Return"),
// "Escape" ("Esc"), "Tab", "Backspace", "Space", "Delete", "Insert", "Home", "End", "PageUp",
// "PageDown", "Left", "Right", "Up", "Down", "Minus", "Equals", "LeftBracket", "RightBracket",
// "Backslash", "Semicolon", "Apostrophe", "Grave" (the key left of 1: ² on a French keyboard),
// "Comma", "Period", "Slash", "CapsLock", "PrintScreen", "ScrollLock", "Pause", "Menu",
// "Ctrl" ("LCtrl"), "RCtrl", "Shift" ("LShift"), "RShift", "Alt" ("LAlt"), "RAlt",
// "Meta" ("LMeta", "Win"), "RMeta". nullptr when unknown.
const KeySpec* findKey(const QString& name);

// The whole table (aliases included), for the documentation and the tests
QList<KeySpec> keyTable();

// One character of `text` typed on a US keyboard: its key and whether Shift is held.
// False when the character has no key there (accents, emoji...).
bool keyForChar(QChar c, KeySpec& key, bool& shift);

enum Modifier
{
    ModCtrl = 1,
    ModShift = 2,
    ModAlt = 4,
    ModMeta = 8,
};

// "Ctrl+Shift", ["Ctrl", "Shift"] or absent/null (none) -> mask. Names: Ctrl, Shift, Alt,
// Meta (Win), case-insensitive.
bool parseModifiers(const QJsonValue& value, int& mask, QString& error);

// The `reset` guard: only the test build's own organisation can ever be wiped.
constexpr const char* kTestOrganization = "LavArtemis Test";
bool isWipeableOrganization(const QString& organization);

// The folder named exactly `organization` that holds `standardPath` (the path itself, its
// parent or its grandparent: Qt puts <org>/<app>[/cache]), or empty. Always empty for an
// organisation isWipeableOrganization() refuses.
QString organizationFolder(const QString& standardPath, const QString& organization);

// "Software\\LavArtemis Test" for the test organisation, empty for any other.
QString registryKeyFor(const QString& organization);

// Deletes `path` and everything under it without following links: a symbolic link or a
// junction inside is removed as a link, its target untouched. False (nothing deleted) when
// `path` itself is a link; false when something could not be removed.
bool removeTreeNoFollow(const QString& path);

// `screenshot` paths: absolute and on a local drive (no \\server\share, //server, \\?\ ...)
bool isLocalAbsolutePath(const QString& path);

}
