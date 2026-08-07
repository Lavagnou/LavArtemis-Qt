#include "keymacromanager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>
#include <QtDebug>

#include <Limelight.h>

#define MACRO_FILE "keyboard-macros.json"

// How long the keys stay held before being released, mirroring Android's
// GameMenu.KEY_UP_DELAY. Some hosts drop a chord that goes down and up within
// the same frame.
#define KEY_UP_DELAY_MS 100

KeyMacroManager* KeyMacroManager::s_Instance = nullptr;

namespace {

struct VkName {
    const char* name;
    int code;
};

// Extracted from the Android client's utils/KeyMapper.java so a macro file
// written for one client works on the other unchanged.
const VkName k_VkNames[] = {
    { "VK_LBUTTON", 0x01 },
    { "VK_RBUTTON", 0x02 },
    { "VK_CANCEL", 0x03 },
    { "VK_MBUTTON", 0x04 },
    { "VK_XBUTTON1", 0x05 },
    { "VK_XBUTTON2", 0x06 },
    { "VK_BACK", 0x08 },
    { "VK_TAB", 0x09 },
    { "VK_CLEAR", 0x0C },
    { "VK_RETURN", 0x0D },
    { "VK_SHIFT", 0x10 },
    { "VK_CONTROL", 0x11 },
    { "VK_MENU", 0x12 },
    { "VK_PAUSE", 0x13 },
    { "VK_CAPITAL", 0x14 },
    { "VK_KANA", 0x15 },
    { "VK_HANGUL", 0x15 },
    { "VK_IME_ON", 0x16 },
    { "VK_JUNJA", 0x17 },
    { "VK_FINAL", 0x18 },
    { "VK_HANJA", 0x19 },
    { "VK_KANJI", 0x19 },
    { "VK_IME_OFF", 0x1A },
    { "VK_ESCAPE", 0x1B },
    { "VK_CONVERT", 0x1C },
    { "VK_NONCONVERT", 0x1D },
    { "VK_ACCEPT", 0x1E },
    { "VK_MODECHANGE", 0x1F },
    { "VK_SPACE", 0x20 },
    { "VK_PRIOR", 0x21 },
    { "VK_NEXT", 0x22 },
    { "VK_END", 0x23 },
    { "VK_HOME", 0x24 },
    { "VK_LEFT", 0x25 },
    { "VK_UP", 0x26 },
    { "VK_RIGHT", 0x27 },
    { "VK_DOWN", 0x28 },
    { "VK_SELECT", 0x29 },
    { "VK_PRINT", 0x2A },
    { "VK_EXECUTE", 0x2B },
    { "VK_SNAPSHOT", 0x2C },
    { "VK_INSERT", 0x2D },
    { "VK_DELETE", 0x2E },
    { "VK_HELP", 0x2F },
    { "VK_0", 0x30 },
    { "VK_1", 0x31 },
    { "VK_2", 0x32 },
    { "VK_3", 0x33 },
    { "VK_4", 0x34 },
    { "VK_5", 0x35 },
    { "VK_6", 0x36 },
    { "VK_7", 0x37 },
    { "VK_8", 0x38 },
    { "VK_9", 0x39 },
    { "VK_A", 0x41 },
    { "VK_B", 0x42 },
    { "VK_C", 0x43 },
    { "VK_D", 0x44 },
    { "VK_E", 0x45 },
    { "VK_F", 0x46 },
    { "VK_G", 0x47 },
    { "VK_H", 0x48 },
    { "VK_I", 0x49 },
    { "VK_J", 0x4A },
    { "VK_K", 0x4B },
    { "VK_L", 0x4C },
    { "VK_M", 0x4D },
    { "VK_N", 0x4E },
    { "VK_O", 0x4F },
    { "VK_P", 0x50 },
    { "VK_Q", 0x51 },
    { "VK_R", 0x52 },
    { "VK_S", 0x53 },
    { "VK_T", 0x54 },
    { "VK_U", 0x55 },
    { "VK_V", 0x56 },
    { "VK_W", 0x57 },
    { "VK_X", 0x58 },
    { "VK_Y", 0x59 },
    { "VK_Z", 0x5A },
    { "VK_LWIN", 0x5B },
    { "VK_RWIN", 0x5C },
    { "VK_APPS", 0x5D },
    { "VK_SLEEP", 0x5F },
    { "VK_NUMPAD0", 0x60 },
    { "VK_NUMPAD1", 0x61 },
    { "VK_NUMPAD2", 0x62 },
    { "VK_NUMPAD3", 0x63 },
    { "VK_NUMPAD4", 0x64 },
    { "VK_NUMPAD5", 0x65 },
    { "VK_NUMPAD6", 0x66 },
    { "VK_NUMPAD7", 0x67 },
    { "VK_NUMPAD8", 0x68 },
    { "VK_NUMPAD9", 0x69 },
    { "VK_MULTIPLY", 0x6A },
    { "VK_ADD", 0x6B },
    { "VK_SEPARATOR", 0x6C },
    { "VK_SUBTRACT", 0x6D },
    { "VK_DECIMAL", 0x6E },
    { "VK_DIVIDE", 0x6F },
    { "VK_F1", 0x70 },
    { "VK_F2", 0x71 },
    { "VK_F3", 0x72 },
    { "VK_F4", 0x73 },
    { "VK_F5", 0x74 },
    { "VK_F6", 0x75 },
    { "VK_F7", 0x76 },
    { "VK_F8", 0x77 },
    { "VK_F9", 0x78 },
    { "VK_F10", 0x79 },
    { "VK_F11", 0x7A },
    { "VK_F12", 0x7B },
    { "VK_F13", 0x7C },
    { "VK_F14", 0x7D },
    { "VK_F15", 0x7E },
    { "VK_F16", 0x7F },
    { "VK_F17", 0x80 },
    { "VK_F18", 0x81 },
    { "VK_F19", 0x82 },
    { "VK_F20", 0x83 },
    { "VK_F21", 0x84 },
    { "VK_F22", 0x85 },
    { "VK_F23", 0x86 },
    { "VK_F24", 0x87 },
    { "VK_NUMLOCK", 0x90 },
    { "VK_SCROLL", 0x91 },
    { "VK_LSHIFT", 0xA0 },
    { "VK_RSHIFT", 0xA1 },
    { "VK_LCONTROL", 0xA2 },
    { "VK_RCONTROL", 0xA3 },
    { "VK_LMENU", 0xA4 },
    { "VK_RMENU", 0xA5 },
    { "VK_BROWSER_BACK", 0xA6 },
    { "VK_BROWSER_FORWARD", 0xA7 },
    { "VK_BROWSER_REFRESH", 0xA8 },
    { "VK_BROWSER_STOP", 0xA9 },
    { "VK_BROWSER_SEARCH", 0xAA },
    { "VK_BROWSER_FAVORITES", 0xAB },
    { "VK_BROWSER_HOME", 0xAC },
    { "VK_VOLUME_MUTE", 0xAD },
    { "VK_VOLUME_DOWN", 0xAE },
    { "VK_VOLUME_UP", 0xAF },
    { "VK_MEDIA_NEXT_TRACK", 0xB0 },
    { "VK_MEDIA_PREV_TRACK", 0xB1 },
    { "VK_MEDIA_STOP", 0xB2 },
    { "VK_MEDIA_PLAY_PAUSE", 0xB3 },
    { "VK_LAUNCH_MAIL", 0xB4 },
    { "VK_LAUNCH_MEDIA_SELECT", 0xB5 },
    { "VK_LAUNCH_APP1", 0xB6 },
    { "VK_LAUNCH_APP2", 0xB7 },
    { "VK_OEM_1", 0xBA },
    { "VK_OEM_PLUS", 0xBB },
    { "VK_OEM_COMMA", 0xBC },
    { "VK_OEM_MINUS", 0xBD },
    { "VK_OEM_PERIOD", 0xBE },
    { "VK_OEM_2", 0xBF },
    { "VK_OEM_3", 0xC0 },
    { "VK_OEM_4", 0xDB },
    { "VK_OEM_5", 0xDC },
    { "VK_OEM_6", 0xDD },
    { "VK_OEM_7", 0xDE },
    { "VK_OEM_8", 0xDF },
    { "VK_OEM_102", 0xE2 },
    { "VK_PROCESSKEY", 0xE5 },
    { "VK_PACKET", 0xE7 },
    { "VK_ATTN", 0xF6 },
    { "VK_CRSEL", 0xF7 },
    { "VK_EXSEL", 0xF8 },
    { "VK_EREOF", 0xF9 },
    { "VK_PLAY", 0xFA },
    { "VK_ZOOM", 0xFB },
    { "VK_NONAME", 0xFC },
    { "VK_PA1", 0xFD },
    { "VK_OEM_CLEAR", 0xFE },
};

} // anonymous namespace

KeyMacroManager::KeyMacroManager(QObject* parent)
    : QObject(parent)
{
    m_BuiltIn = builtInMacros();
    reload();
}

KeyMacroManager* KeyMacroManager::instance()
{
    if (s_Instance == nullptr) {
        s_Instance = new KeyMacroManager();
    }
    return s_Instance;
}

KeyMacroManager* KeyMacroManager::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    KeyMacroManager* manager = instance();
    QQmlEngine::setObjectOwnership(manager, QQmlEngine::CppOwnership);
    return manager;
}

int KeyMacroManager::lookupKeyCode(const QString& token)
{
    QString trimmed = token.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }

    if (trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        bool ok = false;
        int code = trimmed.mid(2).toInt(&ok, 16);
        return (ok && code > 0 && code <= 0xFF) ? code : -1;
    }

    for (const VkName& entry : k_VkNames) {
        if (trimmed.compare(QLatin1String(entry.name), Qt::CaseInsensitive) == 0) {
            return entry.code;
        }
    }

    return -1;
}

unsigned char KeyMacroManager::modifierForKey(short key)
{
    switch (key) {
    case 0x10: // VK_SHIFT
    case 0xA0: // VK_LSHIFT
    case 0xA1: // VK_RSHIFT
        return MODIFIER_SHIFT;
    case 0x11: // VK_CONTROL
    case 0xA2: // VK_LCONTROL
    case 0xA3: // VK_RCONTROL
        return MODIFIER_CTRL;
    case 0x12: // VK_MENU
    case 0xA4: // VK_LMENU
    case 0xA5: // VK_RMENU
        return MODIFIER_ALT;
    case 0x5B: // VK_LWIN
    case 0x5C: // VK_RWIN
        return MODIFIER_META;
    default:
        return 0;
    }
}

QVector<KeyMacroManager::Macro> KeyMacroManager::builtInMacros()
{
    // Same set the Android client offers, minus the ones that only make sense
    // on a touch device. These are combinations the local OS would otherwise
    // intercept before Moonlight could forward them.
    struct Preset {
        const char* id;
        const char* name;
        QVector<short> keys;
    };

    const Preset presets[] = {
        { "esc",              QT_TR_NOOP("Escape"),              { 0x1B } },
        { "f11",              QT_TR_NOOP("F11"),                 { 0x7A } },
        { "alt_f4",           QT_TR_NOOP("Alt+F4"),              { 0xA4, 0x73 } },
        { "alt_enter",        QT_TR_NOOP("Alt+Enter"),           { 0xA4, 0x0D } },
        { "ctrl_v",           QT_TR_NOOP("Ctrl+V"),              { 0xA2, 0x56 } },
        { "win",              QT_TR_NOOP("Windows key"),         { 0x5B } },
        { "win_d",            QT_TR_NOOP("Win+D (show desktop)"),{ 0x5B, 0x44 } },
        { "win_g",            QT_TR_NOOP("Win+G (game bar)"),    { 0x5B, 0x47 } },
        { "ctrl_alt_del",     QT_TR_NOOP("Ctrl+Alt+Del"),        { 0xA2, 0xA4, 0x2E } },
        { "ctrl_shift_esc",   QT_TR_NOOP("Ctrl+Shift+Esc (task manager)"), { 0xA2, 0xA0, 0x1B } },
        { "ctrl_alt_tab",     QT_TR_NOOP("Ctrl+Alt+Tab"),        { 0xA2, 0xA4, 0x09 } },
        { "shift_tab",        QT_TR_NOOP("Shift+Tab"),           { 0xA0, 0x09 } },
        { "win_shift_left",   QT_TR_NOOP("Win+Shift+Left"),      { 0x5B, 0xA0, 0x25 } },
        { "ctrl_alt_shift_f1",  QT_TR_NOOP("Ctrl+Alt+Shift+F1"), { 0xA2, 0xA4, 0xA0, 0x70 } },
        { "ctrl_alt_shift_f12", QT_TR_NOOP("Ctrl+Alt+Shift+F12"),{ 0xA2, 0xA4, 0xA0, 0x7B } },
        { "win_alt_b",        QT_TR_NOOP("Win+Alt+B (HDR toggle)"), { 0x5B, 0xA4, 0x42 } },
    };

    QVector<Macro> macros;
    for (const Preset& preset : presets) {
        Macro macro;
        macro.id = QLatin1String("builtin:") + QLatin1String(preset.id);
        macro.name = tr(preset.name);
        macro.keys = preset.keys;
        macro.builtIn = true;
        macros.append(macro);
    }

    return macros;
}

QString KeyMacroManager::macroFilePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
           QLatin1String("/" MACRO_FILE);
}

void KeyMacroManager::reload()
{
    m_Custom.clear();

    QFile file(macroFilePath());
    if (!file.exists()) {
        // Not having any custom macros is the normal case.
        emit macrosChanged();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        emit macroFileError(tr("Unable to read %1").arg(file.fileName()));
        emit macrosChanged();
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        emit macroFileError(tr("%1 is not valid JSON: %2")
                                .arg(file.fileName(), error.errorString()));
        emit macrosChanged();
        return;
    }

    const QJsonArray entries = doc.object().value(QLatin1String("data")).toArray();
    int index = 0;
    for (const QJsonValue& entry : entries) {
        QJsonObject obj = entry.toObject();

        Macro macro;
        macro.builtIn = false;
        macro.id = QLatin1String("custom:") +
                   (obj.value(QLatin1String("id")).toString().isEmpty()
                        ? QString::number(index)
                        : obj.value(QLatin1String("id")).toString());
        macro.name = obj.value(QLatin1String("name")).toString();
        index++;

        if (macro.name.isEmpty()) {
            continue;
        }

        bool valid = true;
        const QJsonArray keys = obj.value(QLatin1String("keys")).toArray();
        for (const QJsonValue& key : keys) {
            int code = lookupKeyCode(key.toString());
            if (code < 0) {
                emit macroFileError(tr("Macro '%1' uses an unknown key: %2")
                                        .arg(macro.name, key.toString()));
                valid = false;
                break;
            }
            macro.keys.append((short)code);
        }

        // Skip the bad entry rather than dropping the whole file, so one typo
        // doesn't cost the user every macro they wrote.
        if (valid && !macro.keys.isEmpty()) {
            m_Custom.append(macro);
        }
    }

    emit macrosChanged();
}

QVariantList KeyMacroManager::macros() const
{
    QVariantList list;

    auto append = [&list](const QVector<Macro>& macros) {
        for (const Macro& macro : macros) {
            QVariantMap entry;
            entry.insert(QLatin1String("id"), macro.id);
            entry.insert(QLatin1String("name"), macro.name);
            entry.insert(QLatin1String("builtIn"), macro.builtIn);
            list.append(entry);
        }
    };

    append(m_BuiltIn);
    append(m_Custom);

    return list;
}

const KeyMacroManager::Macro* KeyMacroManager::findMacro(const QString& id) const
{
    for (const Macro& macro : m_BuiltIn) {
        if (macro.id == id) {
            return &macro;
        }
    }
    for (const Macro& macro : m_Custom) {
        if (macro.id == id) {
            return &macro;
        }
    }
    return nullptr;
}

void KeyMacroManager::sendMacro(const QString& id)
{
    const Macro* macro = findMacro(id);
    if (macro == nullptr) {
        qWarning() << "KeyMacroManager: unknown macro" << id;
        return;
    }

    QVector<short> keys = macro->keys;

    // Press in order, building up the modifier state as we go. Ctrl goes down
    // with no modifier set, and every key after it carries MODIFIER_CTRL. This
    // is what the Android client does (Game.java:2260) and what hosts expect.
    unsigned char modifiers = 0;
    for (short key : keys) {
        LiSendKeyboardEvent(0x8000 | key, KEY_ACTION_DOWN, modifiers);
        modifiers |= modifierForKey(key);
    }

    // Release in reverse after a beat, clearing each key's own modifier bit
    // before its release so the flags stay consistent on the way back down.
    QTimer::singleShot(KEY_UP_DELAY_MS, this, [keys]() {
        unsigned char modifiers = 0;
        for (short key : keys) {
            modifiers |= modifierForKey(key);
        }

        for (int i = keys.count() - 1; i >= 0; i--) {
            modifiers &= ~modifierForKey(keys.at(i));
            LiSendKeyboardEvent(0x8000 | keys.at(i), KEY_ACTION_UP, modifiers);
        }
    });
}
