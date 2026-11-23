#include "artemissettings.h"
#include "streaming/audio/micstreamer.h"
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

ArtemisSettings* ArtemisSettings::s_instance = nullptr;

ArtemisSettings::ArtemisSettings(QObject *parent)
    : QObject(parent)
    , m_settings(nullptr)
{
    // Create settings directory if it doesn't exist
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir configDir(configPath);
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }

    // Initialize settings
    QString settingsPath = configPath + "/artemis-settings.ini";
    m_settings = new QSettings(settingsPath, QSettings::IniFormat, this);

    // Load defaults first, then load saved settings
    loadDefaults();
    load();

    qDebug() << "ArtemisSettings: Initialized with config at" << settingsPath;
}

ArtemisSettings* ArtemisSettings::instance()
{
    if (!s_instance) {
        s_instance = new ArtemisSettings();
    }
    return s_instance;
}

ArtemisSettings* ArtemisSettings::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    // We hand out the process-wide singleton, which C++ callers reach through
    // instance() without going near the QML engine. Claim ownership explicitly
    // so engine teardown doesn't delete it and leave s_instance dangling.
    ArtemisSettings* settings = instance();
    QQmlEngine::setObjectOwnership(settings, QQmlEngine::CppOwnership);
    return settings;
}

void ArtemisSettings::save()
{
    if (!m_settings) {
        return;
    }

    qDebug() << "ArtemisSettings: Saving settings";

    // Clipboard sync settings
    m_settings->beginGroup("ClipboardSync");
    m_settings->setValue("enabled", m_clipboardSyncEnabled);
    m_settings->setValue("bidirectional", m_clipboardSyncBidirectional);
    m_settings->setValue("maxSize", m_clipboardSyncMaxSize);
    m_settings->setValue("notifications", m_clipboardSyncNotifications);
    m_settings->endGroup();

    // Groups of the Artemis settings removed by the triage of 2026-11-23: nothing reads them
    // any more, and a file that still has them loads fine (unknown keys are ignored)
    for (const char* group : {"ServerCommands", "OTPPairing", "ClientDisplay",
                              "AppOrdering", "Permissions", "InputOnly"}) {
        m_settings->remove(QLatin1String(group));
    }

    // LavArtemis streaming hot path
    m_settings->beginGroup("LavArtemis");
    m_settings->setValue("perfCsvLogging", m_perfCsvLoggingEnabled);
    m_settings->setValue("maxPendingAudioMs", m_maxPendingAudioMs);
    m_settings->setValue("genericGamepadFallback", m_genericGamepadFallbackEnabled);
    m_settings->setValue("micPassthrough", m_micPassthroughEnabled);
    m_settings->setValue("micDeviceName", m_micDeviceName);
    m_settings->endGroup();

    // Software update settings
    m_settings->beginGroup("SoftwareUpdate");
    m_settings->setValue("autoUpdateEnabled", m_autoUpdateEnabled);
    m_settings->setValue("uiTheme", m_uiTheme);
    m_settings->setValue("uiThemeMode", m_uiThemeMode);
    // The way back to the legacy interface is gone (LavArtemis Dev always opens gui/v2)
    m_settings->remove("uiLegacy");
    m_settings->setValue("uiExpertMode", m_uiExpertMode);
    // CI prereleases: replaced by the LavArtemis Dev app
    m_settings->remove("autoUpdatePrerelease");
    m_settings->endGroup();

    m_settings->sync();
}

void ArtemisSettings::load()
{
    if (!m_settings) {
        return;
    }

    qDebug() << "ArtemisSettings: Loading settings";

    // Clipboard sync settings
    m_settings->beginGroup("ClipboardSync");
    m_clipboardSyncEnabled = m_settings->value("enabled", false).toBool();
    m_clipboardSyncBidirectional = m_settings->value("bidirectional", true).toBool();
    m_clipboardSyncMaxSize = m_settings->value("maxSize", 1048576).toInt(); // 1MB default
    m_clipboardSyncNotifications = m_settings->value("notifications", true).toBool();
    m_settings->endGroup();

    // LavArtemis streaming hot path
    m_settings->beginGroup("LavArtemis");
    m_perfCsvLoggingEnabled = m_settings->value("perfCsvLogging", false).toBool();
    // Clamped to the same 10-100 ms range the Android client exposes.
    m_maxPendingAudioMs = qBound(10, m_settings->value("maxPendingAudioMs", 30).toInt(), 100);
    m_genericGamepadFallbackEnabled = m_settings->value("genericGamepadFallback", true).toBool();
    m_micPassthroughEnabled = m_settings->value("micPassthrough", false).toBool();
    m_micDeviceName = m_settings->value("micDeviceName", QString()).toString();
    m_settings->endGroup();

    // Software update settings
    m_settings->beginGroup("SoftwareUpdate");
    m_autoUpdateEnabled = m_settings->value("autoUpdateEnabled", true).toBool();
    m_uiTheme = m_settings->value("uiTheme", QStringLiteral("braise")).toString();
    m_uiThemeMode = m_settings->value("uiThemeMode", 0).toInt();
    m_uiExpertMode = m_settings->value("uiExpertMode", false).toBool();
    m_settings->endGroup();
}

void ArtemisSettings::loadDefaults()
{
    // Clipboard sync defaults
    m_clipboardSyncEnabled = false;
    m_clipboardSyncBidirectional = true;
    m_clipboardSyncMaxSize = 1048576; // 1MB
    m_clipboardSyncNotifications = true;

    // LavArtemis hot path defaults. 30 ms keeps the previous hardcoded audio
    // behaviour; the Android client defaults to 40 ms but uses a blocking
    // AudioTrack write rather than SDL's queue.
    m_perfCsvLoggingEnabled = false;
    m_maxPendingAudioMs = 30;

    // On by default: without it, a gamepad SDL doesn't recognise is silently
    // dropped and the user has no way to tell why.
    m_genericGamepadFallbackEnabled = true;

    // Off by default: sending the microphone is something to opt into, per machine.
    m_micPassthroughEnabled = false;
    m_micDeviceName.clear();

    // Check for updates at startup
    m_autoUpdateEnabled = true;

    m_uiTheme = QStringLiteral("braise");
    m_uiThemeMode = 0;
    m_uiExpertMode = false;
}

void ArtemisSettings::setUiTheme(const QString &theme)
{
    if (m_uiTheme != theme) {
        m_uiTheme = theme;
        emit uiThemeChanged();
    }
}

void ArtemisSettings::setUiThemeMode(int mode)
{
    if (m_uiThemeMode != mode) {
        m_uiThemeMode = mode;
        emit uiThemeModeChanged();
    }
}

void ArtemisSettings::setUiExpertMode(bool expert)
{
    if (m_uiExpertMode != expert) {
        m_uiExpertMode = expert;
        emit uiExpertModeChanged();
    }
}

// Clipboard sync setters
void ArtemisSettings::setClipboardSyncEnabled(bool enabled)
{
    if (m_clipboardSyncEnabled != enabled) {
        m_clipboardSyncEnabled = enabled;
        emit clipboardSyncEnabledChanged();
    }
}

void ArtemisSettings::setClipboardSyncBidirectional(bool bidirectional)
{
    if (m_clipboardSyncBidirectional != bidirectional) {
        m_clipboardSyncBidirectional = bidirectional;
        emit clipboardSyncBidirectionalChanged();
    }
}

void ArtemisSettings::setClipboardSyncMaxSize(int maxSize)
{
    if (m_clipboardSyncMaxSize != maxSize) {
        m_clipboardSyncMaxSize = maxSize;
        emit clipboardSyncMaxSizeChanged();
    }
}

void ArtemisSettings::setClipboardSyncNotifications(bool show)
{
    if (m_clipboardSyncNotifications != show) {
        m_clipboardSyncNotifications = show;
        emit clipboardSyncNotificationsChanged();
    }
}

// LavArtemis hot path setters
void ArtemisSettings::setPerfCsvLoggingEnabled(bool enabled)
{
    if (m_perfCsvLoggingEnabled != enabled) {
        m_perfCsvLoggingEnabled = enabled;
        emit perfCsvLoggingEnabledChanged();
    }
}

void ArtemisSettings::setMaxPendingAudioMs(int ms)
{
    ms = qBound(10, ms, 100);
    if (m_maxPendingAudioMs != ms) {
        m_maxPendingAudioMs = ms;
        emit maxPendingAudioMsChanged();
    }
}

void ArtemisSettings::openPerfCsvFolder() const
{
    // Same folder as FFmpegVideoDecoder::writePerfCsvRow()
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

// Microphone passthrough setters
void ArtemisSettings::setMicPassthroughEnabled(bool enabled)
{
    if (m_micPassthroughEnabled != enabled) {
        m_micPassthroughEnabled = enabled;
        emit micPassthroughEnabledChanged();
    }
}

void ArtemisSettings::setMicDeviceName(const QString &name)
{
    if (m_micDeviceName != name) {
        m_micDeviceName = name;
        emit micDeviceNameChanged();
    }
}

QStringList ArtemisSettings::microphoneDevices() const
{
    return MicStreamer::availableDevices();
}

// Input setters
void ArtemisSettings::setGenericGamepadFallbackEnabled(bool enabled)
{
    if (m_genericGamepadFallbackEnabled != enabled) {
        m_genericGamepadFallbackEnabled = enabled;
        emit genericGamepadFallbackEnabledChanged();
    }
}

// Software update setters
void ArtemisSettings::setAutoUpdateEnabled(bool enabled)
{
    if (m_autoUpdateEnabled != enabled) {
        m_autoUpdateEnabled = enabled;
        emit autoUpdateEnabledChanged();
    }
}
