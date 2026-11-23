#pragma once

#include <QObject>
#include <QSettings>
#include <QQmlEngine>

/**
 * @brief LavArtemis settings that belong to this machine, not to a settings profile
 *
 * Clipboard sync, the microphone, the audio buffer, performance logging, the
 * gamepad fallback, updates and the interface (theme, expert mode). Stored in
 * artemis-settings.ini; the setters do not persist, call save().
 *
 * The Artemis properties nothing read any more (server commands, OTP pairing,
 * the client-display duplicates of StreamingPreferences, app ordering,
 * permissions, input-only mode, CI prereleases) were removed in the settings
 * triage of 2026-11-23: their groups are dropped from the file on the next save.
 */
class ArtemisSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Clipboard sync settings
    Q_PROPERTY(bool clipboardSyncEnabled READ clipboardSyncEnabled WRITE setClipboardSyncEnabled NOTIFY clipboardSyncEnabledChanged)
    Q_PROPERTY(bool clipboardSyncBidirectional READ clipboardSyncBidirectional WRITE setClipboardSyncBidirectional NOTIFY clipboardSyncBidirectionalChanged)
    Q_PROPERTY(int clipboardSyncMaxSize READ clipboardSyncMaxSize WRITE setClipboardSyncMaxSize NOTIFY clipboardSyncMaxSizeChanged)
    Q_PROPERTY(bool clipboardSyncNotifications READ clipboardSyncNotifications WRITE setClipboardSyncNotifications NOTIFY clipboardSyncNotificationsChanged)

    // LavArtemis streaming hot path
    Q_PROPERTY(bool perfCsvLoggingEnabled READ perfCsvLoggingEnabled WRITE setPerfCsvLoggingEnabled NOTIFY perfCsvLoggingEnabledChanged)
    Q_PROPERTY(int maxPendingAudioMs READ maxPendingAudioMs WRITE setMaxPendingAudioMs NOTIFY maxPendingAudioMsChanged)

    // Microphone passthrough (LavApollo plays it into a virtual microphone on the host)
    Q_PROPERTY(bool micPassthroughEnabled READ micPassthroughEnabled WRITE setMicPassthroughEnabled NOTIFY micPassthroughEnabledChanged)
    Q_PROPERTY(QString micDeviceName READ micDeviceName WRITE setMicDeviceName NOTIFY micDeviceNameChanged)

    // Input
    Q_PROPERTY(bool genericGamepadFallbackEnabled READ genericGamepadFallbackEnabled WRITE setGenericGamepadFallbackEnabled NOTIFY genericGamepadFallbackEnabledChanged)

    // Software update settings
    Q_PROPERTY(bool autoUpdateEnabled READ autoUpdateEnabled WRITE setAutoUpdateEnabled NOTIFY autoUpdateEnabledChanged)

    // Redesigned interface (gui/v2, docs/superpowers/specs/2026-11-23-ui-redesign-design.md):
    // theme ("braise", "affiche", "ludique") and light/dark mode (0 system, 1 light, 2 dark).
    // Machine-wide, like the language: not per profile.
    Q_PROPERTY(QString uiTheme READ uiTheme WRITE setUiTheme NOTIFY uiThemeChanged)
    Q_PROPERTY(int uiThemeMode READ uiThemeMode WRITE setUiThemeMode NOTIFY uiThemeModeChanged)
    // Réglages v2 (gui/v2/SettingsView2.qml): shows the rows triaged as "expert". Machine-wide.
    Q_PROPERTY(bool uiExpertMode READ uiExpertMode WRITE setUiExpertMode NOTIFY uiExpertModeChanged)

public:
    static ArtemisSettings* instance();
    static ArtemisSettings* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    Q_INVOKABLE void save();
    Q_INVOKABLE void load();

    // Clipboard sync getters/setters
    bool clipboardSyncEnabled() const { return m_clipboardSyncEnabled; }
    void setClipboardSyncEnabled(bool enabled);

    bool clipboardSyncBidirectional() const { return m_clipboardSyncBidirectional; }
    void setClipboardSyncBidirectional(bool bidirectional);

    int clipboardSyncMaxSize() const { return m_clipboardSyncMaxSize; }
    void setClipboardSyncMaxSize(int maxSize);

    bool clipboardSyncNotifications() const { return m_clipboardSyncNotifications; }
    void setClipboardSyncNotifications(bool show);

    // LavArtemis hot path getters/setters
    bool perfCsvLoggingEnabled() const { return m_perfCsvLoggingEnabled; }
    void setPerfCsvLoggingEnabled(bool enabled);
    int maxPendingAudioMs() const { return m_maxPendingAudioMs; }
    void setMaxPendingAudioMs(int ms);

    // Folder the performance CSV files go to (FFmpegVideoDecoder::writePerfCsvRow())
    Q_INVOKABLE void openPerfCsvFolder() const;

    // Microphone passthrough getters/setters
    bool micPassthroughEnabled() const { return m_micPassthroughEnabled; }
    void setMicPassthroughEnabled(bool enabled);
    QString micDeviceName() const { return m_micDeviceName; }
    void setMicDeviceName(const QString &name);

    // Recording devices SDL can open, for the microphone picker. Empty name = system default.
    Q_INVOKABLE QStringList microphoneDevices() const;

    // Input getters/setters
    bool genericGamepadFallbackEnabled() const { return m_genericGamepadFallbackEnabled; }
    void setGenericGamepadFallbackEnabled(bool enabled);

    // Software update getters/setters
    bool autoUpdateEnabled() const { return m_autoUpdateEnabled; }
    void setAutoUpdateEnabled(bool enabled);

    // Interface getters/setters
    QString uiTheme() const { return m_uiTheme; }
    void setUiTheme(const QString &theme);
    int uiThemeMode() const { return m_uiThemeMode; }
    void setUiThemeMode(int mode);
    bool uiExpertMode() const { return m_uiExpertMode; }
    void setUiExpertMode(bool expert);

signals:
    // Clipboard sync signals
    void clipboardSyncEnabledChanged();
    void clipboardSyncBidirectionalChanged();
    void clipboardSyncMaxSizeChanged();
    void clipboardSyncNotificationsChanged();

    // LavArtemis hot path signals
    void perfCsvLoggingEnabledChanged();
    void maxPendingAudioMsChanged();

    // Microphone passthrough signals
    void micPassthroughEnabledChanged();
    void micDeviceNameChanged();

    // Input signals
    void genericGamepadFallbackEnabledChanged();

    // Software update and interface signals
    void autoUpdateEnabledChanged();
    void uiThemeChanged();
    void uiThemeModeChanged();
    void uiExpertModeChanged();

private:
    explicit ArtemisSettings(QObject *parent = nullptr);

    void loadDefaults();

private:
    static ArtemisSettings* s_instance;
    QSettings *m_settings;

    // Clipboard sync settings
    bool m_clipboardSyncEnabled;
    bool m_clipboardSyncBidirectional;
    int m_clipboardSyncMaxSize;
    bool m_clipboardSyncNotifications;

    // LavArtemis streaming hot path
    bool m_perfCsvLoggingEnabled;
    int m_maxPendingAudioMs;

    // Input
    bool m_genericGamepadFallbackEnabled;

    // Microphone passthrough
    bool m_micPassthroughEnabled;
    QString m_micDeviceName;

    // Software update and interface
    bool m_autoUpdateEnabled;
    QString m_uiTheme;
    int m_uiThemeMode;
    bool m_uiExpertMode;
};
