#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVector>

/**
 * @brief Keyboard shortcuts and macros that can be sent to the host.
 *
 * The desktop counterpart of the Artemis "send keys" menu (GameMenu.java:155)
 * and its custom shortcut file (utils/KeyConfigHelper.java).
 *
 * Built-in presets cover the combinations a client can't otherwise deliver,
 * because the local OS swallows them before Moonlight sees them: Alt+F4,
 * Ctrl+Alt+Tab, Win+G and friends.
 *
 * Custom macros are read from a JSON file whose format matches Android's, so
 * one file works on both:
 *
 *   {
 *     "data": [
 *       { "id": "1", "name": "Open task manager",
 *         "keys": ["VK_LCONTROL", "VK_LSHIFT", "VK_ESCAPE"] }
 *     ]
 *   }
 *
 * Keys are either a VK_ name or a literal hex code such as "0x1B".
 */
class KeyMacroManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList macros READ macros NOTIFY macrosChanged)
    Q_PROPERTY(QString macroFilePath READ macroFilePath CONSTANT)

public:
    static KeyMacroManager* instance();
    static KeyMacroManager* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    // Built-in presets first, then whatever the macro file defines.
    QVariantList macros() const;
    QString macroFilePath() const;

    Q_INVOKABLE void sendMacro(const QString& id);
    Q_INVOKABLE void reload();

    // Resolves "VK_ESCAPE" or "0x1B" to a virtual key code. Returns -1 if the
    // token is not a name we know and not a valid hex literal.
    static int lookupKeyCode(const QString& token);

signals:
    void macrosChanged();

    // Raised when the macro file exists but could not be used, so the UI can
    // say so instead of silently offering nothing.
    void macroFileError(const QString& message);

private:
    explicit KeyMacroManager(QObject* parent = nullptr);

    struct Macro {
        QString id;
        QString name;
        QVector<short> keys;
        bool builtIn;
    };

    static QVector<Macro> builtInMacros();
    static unsigned char modifierForKey(short key);

    const Macro* findMacro(const QString& id) const;

    static KeyMacroManager* s_Instance;

    QVector<Macro> m_BuiltIn;
    QVector<Macro> m_Custom;
};
