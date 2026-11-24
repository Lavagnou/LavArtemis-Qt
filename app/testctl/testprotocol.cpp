#include "testprotocol.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#ifdef Q_OS_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace TestProtocol {

bool parseRequest(const QByteArray& line, Request& out, QString& error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error = QStringLiteral("malformed JSON: %1").arg(parseError.errorString());
        return false;
    }
    if (!doc.isObject()) {
        error = QStringLiteral("a request is a JSON object");
        return false;
    }

    out.args = doc.object();
    out.id = out.args.value("id");
    if (out.id.isUndefined()) {
        out.id = QJsonValue(QJsonValue::Null);
    }
    const QJsonValue cmd = out.args.value("cmd");
    if (!cmd.isString() || cmd.toString().isEmpty()) {
        out.cmd.clear();
        error = QStringLiteral("missing \"cmd\"");
        return false;
    }
    out.cmd = cmd.toString();
    return true;
}

QByteArray okLine(const QJsonValue& id, const QJsonObject& result)
{
    QJsonObject response = result;
    response["id"] = id;
    response["ok"] = true;
    return QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
}

QByteArray errorLine(const QJsonValue& id, const QString& error)
{
    QJsonObject response;
    response["id"] = id;
    response["ok"] = false;
    response["error"] = error;
    return QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
}

void Dispatcher::add(const QString& cmd, Handler handler)
{
    m_Handlers.insert(cmd, std::move(handler));
}

QByteArray Dispatcher::handleLine(const QByteArray& line)
{
    Request request;
    QString error;
    if (!parseRequest(line, request, error)) {
        // A line that is not a request has no id we could trust
        const QJsonValue id = request.args.isEmpty() ? QJsonValue(QJsonValue::Null) : request.id;
        return errorLine(id, error);
    }

    auto it = m_Handlers.constFind(request.cmd);
    if (it == m_Handlers.constEnd()) {
        return errorLine(request.id, QStringLiteral("unknown command \"%1\"").arg(request.cmd));
    }
    if (m_Running) {
        // Called from a nested event loop of the running command: the caller queues instead
        return errorLine(request.id, QStringLiteral("another command is running"));
    }

    QJsonObject result;
    m_Running = true;
    const bool ok = it.value()(request.args, result, error);
    m_Running = false;
    if (!ok) {
        return errorLine(request.id, error.isEmpty() ? QStringLiteral("failed") : error);
    }
    return okLine(request.id, result);
}

QList<LineBuffer::Line> LineBuffer::feed(const QByteArray& data)
{
    QList<Line> lines;
    m_Buffer += data;
    for (;;) {
        const int newline = m_Buffer.indexOf('\n');
        if (m_Discarding) {
            // The rest of a line already refused: dropped up to its end, no second entry
            if (newline < 0) {
                m_Buffer.clear();
                break;
            }
            m_Buffer.remove(0, newline + 1);
            m_Discarding = false;
            continue;
        }
        if (newline < 0) {
            if (m_Buffer.size() > kMaxLine) {
                lines.append({QByteArray(), true});
                m_Buffer.clear();
                m_Discarding = true;
            }
            break;
        }
        if (newline > kMaxLine) {
            // Complete in this read, but too long all the same
            lines.append({QByteArray(), true});
            m_Buffer.remove(0, newline + 1);
            continue;
        }
        const QByteArray line = m_Buffer.left(newline).trimmed();
        m_Buffer.remove(0, newline + 1);
        if (!line.isEmpty()) {
            lines.append({line, false});
        }
    }
    return lines;
}

void LineBuffer::clear()
{
    m_Buffer.clear();
    m_Discarding = false;
}

QStringList Dispatcher::commands() const
{
    QStringList list = m_Handlers.keys();
    list.sort();
    return list;
}

namespace {

constexpr int kScancodeMask = 1 << 30; // SDLK_SCANCODE_MASK: keycodes of keys without a character

// Qt::Key values (qnamespace.h); this file only links Qt Core, where they are defined too
constexpr KeySpec kKeys[] = {
    {"Enter", Qt::Key_Return, 40, '\r'},
    {"Return", Qt::Key_Return, 40, '\r'},
    {"Escape", Qt::Key_Escape, 41, 27},
    {"Esc", Qt::Key_Escape, 41, 27},
    {"Backspace", Qt::Key_Backspace, 42, '\b'},
    {"Tab", Qt::Key_Tab, 43, '\t'},
    {"Space", Qt::Key_Space, 44, ' '},
    {"Minus", Qt::Key_Minus, 45, '-'},
    {"Equals", Qt::Key_Equal, 46, '='},
    {"LeftBracket", Qt::Key_BracketLeft, 47, '['},
    {"RightBracket", Qt::Key_BracketRight, 48, ']'},
    {"Backslash", Qt::Key_Backslash, 49, '\\'},
    {"Semicolon", Qt::Key_Semicolon, 51, ';'},
    {"Apostrophe", Qt::Key_Apostrophe, 52, '\''},
    {"Grave", Qt::Key_QuoteLeft, 53, '`'},
    {"Comma", Qt::Key_Comma, 54, ','},
    {"Period", Qt::Key_Period, 55, '.'},
    {"Slash", Qt::Key_Slash, 56, '/'},
    {"CapsLock", Qt::Key_CapsLock, 57, 57 | kScancodeMask},
    {"PrintScreen", Qt::Key_Print, 70, 70 | kScancodeMask},
    {"ScrollLock", Qt::Key_ScrollLock, 71, 71 | kScancodeMask},
    {"Pause", Qt::Key_Pause, 72, 72 | kScancodeMask},
    {"Insert", Qt::Key_Insert, 73, 73 | kScancodeMask},
    {"Home", Qt::Key_Home, 74, 74 | kScancodeMask},
    {"PageUp", Qt::Key_PageUp, 75, 75 | kScancodeMask},
    {"Delete", Qt::Key_Delete, 76, 127},
    {"End", Qt::Key_End, 77, 77 | kScancodeMask},
    {"PageDown", Qt::Key_PageDown, 78, 78 | kScancodeMask},
    {"Right", Qt::Key_Right, 79, 79 | kScancodeMask},
    {"Left", Qt::Key_Left, 80, 80 | kScancodeMask},
    {"Down", Qt::Key_Down, 81, 81 | kScancodeMask},
    {"Up", Qt::Key_Up, 82, 82 | kScancodeMask},
    {"Menu", Qt::Key_Menu, 101, 101 | kScancodeMask},
    {"Ctrl", Qt::Key_Control, 224, 224 | kScancodeMask},
    {"LCtrl", Qt::Key_Control, 224, 224 | kScancodeMask},
    {"Shift", Qt::Key_Shift, 225, 225 | kScancodeMask},
    {"LShift", Qt::Key_Shift, 225, 225 | kScancodeMask},
    {"Alt", Qt::Key_Alt, 226, 226 | kScancodeMask},
    {"LAlt", Qt::Key_Alt, 226, 226 | kScancodeMask},
    {"Meta", Qt::Key_Meta, 227, 227 | kScancodeMask},
    {"LMeta", Qt::Key_Meta, 227, 227 | kScancodeMask},
    {"Win", Qt::Key_Meta, 227, 227 | kScancodeMask},
    {"RCtrl", Qt::Key_Control, 228, 228 | kScancodeMask},
    {"RShift", Qt::Key_Shift, 229, 229 | kScancodeMask},
    {"RAlt", Qt::Key_Alt, 230, 230 | kScancodeMask},
    {"RMeta", Qt::Key_Meta, 231, 231 | kScancodeMask},
};

// Letters, digits and function keys follow a pattern; names are built once
struct GeneratedKeys
{
    QList<KeySpec> keys;
    QList<QByteArray> names; // owns the generated names

    GeneratedKeys()
    {
        names.reserve(26 + 10 + 12);
        for (int i = 0; i < 26; i++) {
            names.append(QByteArray(1, char('A' + i)));
        }
        for (int i = 0; i < 10; i++) {
            names.append(QByteArray(1, char('0' + i)));
        }
        for (int i = 1; i <= 12; i++) {
            names.append("F" + QByteArray::number(i));
        }
        int n = 0;
        for (int i = 0; i < 26; i++) {
            keys.append({names[n++].constData(), Qt::Key_A + i, 4 + i, 'a' + i});
        }
        for (int i = 0; i < 10; i++) {
            // HID: 1..9 are 30..38, 0 is 39
            keys.append({names[n++].constData(), Qt::Key_0 + i, i == 0 ? 39 : 29 + i, '0' + i});
        }
        for (int i = 0; i < 12; i++) {
            keys.append({names[n++].constData(), Qt::Key_F1 + i, 58 + i, (58 + i) | kScancodeMask});
        }
        for (const KeySpec& key : kKeys) {
            keys.append(key);
        }
    }
};

const GeneratedKeys& allKeys()
{
    static const GeneratedKeys keys;
    return keys;
}

const KeySpec& keyNamed(const char* name)
{
    const KeySpec* key = findKey(QLatin1String(name));
    Q_ASSERT(key != nullptr);
    return *key;
}

}

const KeySpec* findKey(const QString& name)
{
    if (name.isEmpty()) {
        return nullptr;
    }
    for (const KeySpec& key : allKeys().keys) {
        if (name.compare(QLatin1String(key.name), Qt::CaseInsensitive) == 0) {
            return &key;
        }
    }
    return nullptr;
}

QList<KeySpec> keyTable()
{
    return allKeys().keys;
}

bool keyForChar(QChar c, KeySpec& key, bool& shift)
{
    const ushort u = c.unicode();
    shift = false;
    if (u >= 'a' && u <= 'z') {
        key = keyNamed(QByteArray(1, char(u - 'a' + 'A')).constData());
        return true;
    }
    if (u >= 'A' && u <= 'Z') {
        key = keyNamed(QByteArray(1, char(u)).constData());
        shift = true;
        return true;
    }
    if (u >= '0' && u <= '9') {
        key = keyNamed(QByteArray(1, char(u)).constData());
        return true;
    }

    // US layout: unshifted character, and the shifted one on the same key
    static const struct { char plain; char shifted; const char* key; } kPunctuation[] = {
        {'-', '_', "Minus"}, {'=', '+', "Equals"}, {'[', '{', "LeftBracket"}, {']', '}', "RightBracket"},
        {'\\', '|', "Backslash"}, {';', ':', "Semicolon"}, {'\'', '"', "Apostrophe"}, {'`', '~', "Grave"},
        {',', '<', "Comma"}, {'.', '>', "Period"}, {'/', '?', "Slash"},
        {'1', '!', "1"}, {'2', '@', "2"}, {'3', '#', "3"}, {'4', '$', "4"}, {'5', '%', "5"},
        {'6', '^', "6"}, {'7', '&', "7"}, {'8', '*', "8"}, {'9', '(', "9"}, {'0', ')', "0"},
        {' ', 0, "Space"}, {'\n', 0, "Enter"}, {'\t', 0, "Tab"},
    };
    for (const auto& entry : kPunctuation) {
        if (u == ushort(entry.plain)) {
            key = keyNamed(entry.key);
            return true;
        }
        if (entry.shifted != 0 && u == ushort(entry.shifted)) {
            key = keyNamed(entry.key);
            shift = true;
            return true;
        }
    }
    return false;
}

bool parseModifiers(const QJsonValue& value, int& mask, QString& error)
{
    mask = 0;
    QStringList names;
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    else if (value.isString()) {
        names = value.toString().split('+', Qt::SkipEmptyParts);
    }
    else if (value.isArray()) {
        for (const QJsonValue& entry : value.toArray()) {
            if (!entry.isString()) {
                error = QStringLiteral("modifiers are names: Ctrl, Shift, Alt, Meta");
                return false;
            }
            names.append(entry.toString());
        }
    }
    else {
        error = QStringLiteral("modifiers: \"Ctrl+Shift\" or [\"Ctrl\", \"Shift\"]");
        return false;
    }

    for (const QString& raw : names) {
        const QString name = raw.trimmed().toLower();
        if (name == "ctrl" || name == "control") {
            mask |= ModCtrl;
        }
        else if (name == "shift") {
            mask |= ModShift;
        }
        else if (name == "alt") {
            mask |= ModAlt;
        }
        else if (name == "meta" || name == "win") {
            mask |= ModMeta;
        }
        else {
            error = QStringLiteral("unknown modifier \"%1\" (Ctrl, Shift, Alt, Meta)").arg(raw);
            return false;
        }
    }
    return true;
}

bool isWipeableOrganization(const QString& organization)
{
    // Exact, case-sensitive: never "LavArtemis", "LavArtemis Dev" or anything near
    return organization == QLatin1String(kTestOrganization);
}

QString organizationFolder(const QString& standardPath, const QString& organization)
{
    if (!isWipeableOrganization(organization)) {
        return {};
    }
    QString path = QDir::cleanPath(QDir::fromNativeSeparators(standardPath));
    if (path.isEmpty() || QDir::isRelativePath(path)) {
        return {};
    }

    // <org>, <org>/<app> or <org>/<app>/cache (string work only: the folders may not exist)
    QString current = path;
    for (int level = 0; level <= 2; level++) {
        const QFileInfo info(current);
        if (info.fileName() == organization) {
            return current;
        }
        const QString parent = info.path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

namespace {

bool isLink(const QFileInfo& info)
{
    // A junction is not a symbolic link for Qt, and both look like the folder they point to
    return info.isSymLink() || info.isJunction();
}

bool removeEntry(const QFileInfo& info)
{
    const QString path = info.absoluteFilePath();
    if (isLink(info)) {
        // The link itself: RemoveDirectory on a directory link / junction, DeleteFile otherwise
        return QDir().rmdir(path) || QFile::remove(path);
    }
    if (info.isDir()) {
        bool ok = true;
        const QFileInfoList entries = QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
                                                               QDir::Hidden | QDir::System);
        for (const QFileInfo& entry : entries) {
            ok = removeEntry(entry) && ok;
        }
        return ok && QDir().rmdir(path);
    }
    QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::WriteOwner);
    return QFile::remove(path);
}

}

bool removeTreeNoFollow(const QString& path)
{
    const QFileInfo root(path);
    if (isLink(root)) {
        return false;
    }
    if (!root.exists()) {
        return true;
    }
    return removeEntry(root);
}

bool isLocalAbsolutePath(const QString& path)
{
    if (path.startsWith(QLatin1String("//")) || path.startsWith(QLatin1String("\\\\"))) {
        return false;
    }
#ifdef Q_OS_WIN32
    // A drive letter, then a separator, and not a mapped network drive
    if (!(path.size() >= 3 && path[0].isLetter() && path[0].unicode() < 128 && path[1] == ':' &&
          (path[2] == '/' || path[2] == '\\'))) {
        return false;
    }
    const QString drive = path.left(2) + QLatin1Char('\\');
    return GetDriveTypeW(reinterpret_cast<LPCWSTR>(drive.utf16())) != DRIVE_REMOTE;
#else
    return QDir::isAbsolutePath(path);
#endif
}

QString registryKeyFor(const QString& organization)
{
    if (!isWipeableOrganization(organization)) {
        return {};
    }
    return QStringLiteral("Software\\") + organization;
}

}
