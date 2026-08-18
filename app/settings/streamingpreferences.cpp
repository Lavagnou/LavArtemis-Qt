#include "streamingpreferences.h"
#include "profilemanager.h"
#include "utils.h"

#include <QMetaProperty>
#include <QSettings>
#include <QTranslator>
#include <QCoreApplication>
#include <QLocale>
#include <QReadWriteLock>
#include <QtMath>

#include <QtDebug>

#define SER_STREAMSETTINGS "streamsettings"
#define SER_WIDTH "width"
#define SER_HEIGHT "height"
#define SER_FPS "fps"
#define SER_BITRATE "bitrate"
#define SER_UNLOCK_BITRATE "unlockbitrate"
#define SER_AUTOADJUSTBITRATE "autoadjustbitrate"
#define SER_FULLSCREEN "fullscreen"
#define SER_VSYNC "vsync"
#define SER_GAMEOPTS "gameopts"
#define SER_HOSTAUDIO "hostaudio"
#define SER_MULTICONT "multicontroller"
#define SER_AUDIOCFG "audiocfg"
#define SER_VIDEOCFG "videocfg"
#define SER_HDR "hdr"
#define SER_YUV444 "yuv444"
#define SER_VIDEODEC "videodec"
#define SER_WINDOWMODE "windowmode"
#define SER_MDNS "mdns"
#define SER_QUITAPPAFTER "quitAppAfter"
#define SER_ABSMOUSEMODE "mouseacceleration"
#define SER_ABSTOUCHMODE "abstouchmode"
#define SER_STARTWINDOWED "startwindowed"
#define SER_FRAMEPACING "framepacing"
#define SER_CONNWARNINGS "connwarnings"
#define SER_CONFWARNINGS "confwarnings"
#define SER_UIDISPLAYMODE "uidisplaymode"
#define SER_RICHPRESENCE "richpresence"
#define SER_GAMEPADMOUSE "gamepadmouse"
#define SER_DEFAULTVER "defaultver"
#define SER_PACKETSIZE "packetsize"
#define SER_DETECTNETBLOCKING "detectnetblocking"
#define SER_SHOWPERFOVERLAY "showperfoverlay"
#define SER_SWAPMOUSEBUTTONS "swapmousebuttons"
#define SER_MUTEONFOCUSLOSS "muteonfocusloss"
#define SER_BACKGROUNDGAMEPAD "backgroundgamepad"
#define SER_REVERSESCROLL "reversescroll"
#define SER_SWAPFACEBUTTONS "swapfacebuttons"
#define SER_CAPTURESYSKEYS "capturesyskeys"
#define SER_KEEPAWAKE "keepawake"
#define SER_LANGUAGE "language"
#define SER_RENDERER "renderer"
#define SER_RENDERERBACKEND "rendererbackend"

// Artemis client-side streaming enhancements
#define SER_VIRTUALDISPLAY "virtualdisplay"
#define SER_FRACTIONALREFRESHRATE "fractionalrefreshrate"
#define SER_CUSTOMREFRESHRATE "customrefreshrate"
#define SER_RESOLUTIONSCALING "resolutionscaling"
#define SER_RESOLUTIONSCALEFACTOR "resolutionscalefactor"
#define SER_MULTIDISPLAY "multidisplay"

#define CURRENT_DEFAULT_VER 2

static StreamingPreferences* s_GlobalPrefs;

Q_GLOBAL_STATIC(QReadWriteLock, s_GlobalPrefsLock)

StreamingPreferences::StreamingPreferences(QQmlEngine *qmlEngine)
    : m_QmlEngine(qmlEngine)
{
    reload();

    // Selecting a different profile changes what every key resolves to, so pull
    // the new values in and let anything bound in QML know.
    connect(ProfileManager::instance(), &ProfileManager::activeProfileSwitched,
            this, [this]() {
                reload();
                notifyAllPropertiesChanged();
            });
}

StreamingPreferences* StreamingPreferences::get(QQmlEngine *qmlEngine)
{
    {
        QReadLocker readGuard(s_GlobalPrefsLock);

        // If we have a preference object and it's associated with a QML engine or
        // if the caller didn't specify a QML engine, return the existing object.
        if (s_GlobalPrefs && (s_GlobalPrefs->m_QmlEngine || !qmlEngine)) {
            // The lifetime logic here relies on the QML engine also being a singleton.
            Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            return s_GlobalPrefs;
        }
    }

    {
        QWriteLocker writeGuard(s_GlobalPrefsLock);

        // If we already have an preference object but the QML engine is now available,
        // associate the QML engine with the preferences.
        if (s_GlobalPrefs) {
            if (!s_GlobalPrefs->m_QmlEngine) {
                s_GlobalPrefs->m_QmlEngine = qmlEngine;
            }
            else {
                // We could reach this codepath if another thread raced with us
                // and created the object while we were outside the pref lock.
                Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            }
        }
        else {
            s_GlobalPrefs = new StreamingPreferences(qmlEngine);
        }

        return s_GlobalPrefs;
    }
}

void StreamingPreferences::reload()
{
    ProfileManager* prefs = ProfileManager::instance();

    int defaultVer = prefs->value(SER_DEFAULTVER, 0).toInt();

#ifdef Q_OS_DARWIN
    recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
#else
    // Wayland doesn't support modesetting, so use fullscreen desktop mode
    // unless we have a slow GPU (which can take advantage of wp_viewporter
    // to reduce GPU load with lower resolution video streams).
    if (WMUtils::isRunningWayland() && !WMUtils::isGpuSlow()) {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
    }
    else {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN;
    }
#endif

    width = prefs->value(SER_WIDTH, 1280).toInt();
    height = prefs->value(SER_HEIGHT, 720).toInt();
    fps = prefs->value(SER_FPS, 60).toInt();
    enableYUV444 = prefs->value(SER_YUV444, false).toBool();
    bitrateKbps = prefs->value(SER_BITRATE, getDefaultBitrate(width, height, fps, enableYUV444)).toInt();
    unlockBitrate = prefs->value(SER_UNLOCK_BITRATE, false).toBool();
    autoAdjustBitrate = prefs->value(SER_AUTOADJUSTBITRATE, true).toBool();
    enableVsync = prefs->value(SER_VSYNC, true).toBool();
    gameOptimizations = prefs->value(SER_GAMEOPTS, true).toBool();
    playAudioOnHost = prefs->value(SER_HOSTAUDIO, false).toBool();
    multiController = prefs->value(SER_MULTICONT, true).toBool();
    enableMdns = prefs->value(SER_MDNS, true).toBool();
    quitAppAfter = prefs->value(SER_QUITAPPAFTER, false).toBool();
    absoluteMouseMode = prefs->value(SER_ABSMOUSEMODE, false).toBool();
    absoluteTouchMode = prefs->value(SER_ABSTOUCHMODE, true).toBool();
    framePacing = prefs->value(SER_FRAMEPACING, false).toBool();
    connectionWarnings = prefs->value(SER_CONNWARNINGS, true).toBool();
    configurationWarnings = prefs->value(SER_CONFWARNINGS, true).toBool();
    richPresence = prefs->value(SER_RICHPRESENCE, true).toBool();
    gamepadMouse = prefs->value(SER_GAMEPADMOUSE, true).toBool();
    detectNetworkBlocking = prefs->value(SER_DETECTNETBLOCKING, true).toBool();
    showPerformanceOverlay = prefs->value(SER_SHOWPERFOVERLAY, false).toBool();
    packetSize = prefs->value(SER_PACKETSIZE, 0).toInt();
    swapMouseButtons = prefs->value(SER_SWAPMOUSEBUTTONS, false).toBool();
    muteOnFocusLoss = prefs->value(SER_MUTEONFOCUSLOSS, false).toBool();
    backgroundGamepad = prefs->value(SER_BACKGROUNDGAMEPAD, false).toBool();
    reverseScrollDirection = prefs->value(SER_REVERSESCROLL, false).toBool();
    swapFaceButtons = prefs->value(SER_SWAPFACEBUTTONS, false).toBool();
    keepAwake = prefs->value(SER_KEEPAWAKE, true).toBool();
    enableHdr = prefs->value(SER_HDR, false).toBool();
    captureSysKeysMode = static_cast<CaptureSysKeysMode>(prefs->value(SER_CAPTURESYSKEYS,
                                                         static_cast<int>(CaptureSysKeysMode::CSK_OFF)).toInt());
    audioConfig = static_cast<AudioConfig>(prefs->value(SER_AUDIOCFG,
                                                  static_cast<int>(AudioConfig::AC_STEREO)).toInt());
    videoCodecConfig = static_cast<VideoCodecConfig>(prefs->value(SER_VIDEOCFG,
                                                  static_cast<int>(VideoCodecConfig::VCC_AUTO)).toInt());
    videoDecoderSelection = static_cast<VideoDecoderSelection>(prefs->value(SER_VIDEODEC,
                                                  static_cast<int>(VideoDecoderSelection::VDS_AUTO)).toInt());
    rendererSelection = static_cast<RendererSelection>(prefs->value(SER_RENDERER,
                                                  static_cast<int>(RendererSelection::RS_AUTO)).toInt());
    windowMode = static_cast<WindowMode>(prefs->value(SER_WINDOWMODE,
                                                        // Try to load from the old preference value too
                                                        static_cast<int>(prefs->value(SER_FULLSCREEN, true).toBool() ?
                                                                             recommendedFullScreenMode : WindowMode::WM_WINDOWED)).toInt());
    uiDisplayMode = static_cast<UIDisplayMode>(prefs->value(SER_UIDISPLAYMODE,
                                               static_cast<int>(prefs->value(SER_STARTWINDOWED, true).toBool() ? UIDisplayMode::UI_WINDOWED
                                                                                                                 : UIDisplayMode::UI_MAXIMIZED)).toInt());
    language = static_cast<Language>(prefs->value(SER_LANGUAGE,
                                                    static_cast<int>(Language::LANG_AUTO)).toInt());
    rendererBackend = static_cast<RendererBackend>(prefs->value(SER_RENDERERBACKEND,
                                                    static_cast<int>(RendererBackend::RB_AUTO)).toInt());

    // Artemis client-side streaming enhancements
    useVirtualDisplay = prefs->value(SER_VIRTUALDISPLAY, false).toBool();
    enableFractionalRefreshRate = prefs->value(SER_FRACTIONALREFRESHRATE, false).toBool();
    customRefreshRate = prefs->value(SER_CUSTOMREFRESHRATE, 59.94).toDouble();
    enableResolutionScaling = prefs->value(SER_RESOLUTIONSCALING, false).toBool();
    resolutionScaleFactor = prefs->value(SER_RESOLUTIONSCALEFACTOR, 100).toInt();
    useMultiDisplay = prefs->value(SER_MULTIDISPLAY, false).toBool();


    // Perform default settings updates as required based on last default version
    if (defaultVer < 1) {
#ifdef Q_OS_DARWIN
        // Update window mode setting on macOS from full-screen (old default) to borderless windowed (new default)
        if (windowMode == WindowMode::WM_FULLSCREEN) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
#endif
    }
    if (defaultVer < 2) {
        if (windowMode == WindowMode::WM_FULLSCREEN && WMUtils::isRunningWayland()) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
    }

    // Fixup VCC value to the new settings format with codec and HDR separate
    if (videoCodecConfig == VCC_FORCE_HEVC_HDR_DEPRECATED) {
        videoCodecConfig = VCC_AUTO;
        enableHdr = true;
    }
}

bool StreamingPreferences::retranslate()
{
    static QTranslator* translator = nullptr;

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    if (m_QmlEngine != nullptr) {
        // Dynamic retranslation is not supported until Qt 5.10
        return false;
    }
#endif

    QTranslator* newTranslator = new QTranslator();
    QString languageSuffix = getSuffixFromLanguage(language);

    // Remove the old translator, even if we can't load a new one.
    // Otherwise we'll be stuck with the old translated values instead
    // of defaulting to English.
    if (translator != nullptr) {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    if (newTranslator->load(QString(":/languages/qml_") + languageSuffix)) {
        qInfo() << "Successfully loaded translation for" << languageSuffix;

        translator = newTranslator;
        QCoreApplication::installTranslator(translator);
    }
    else {
        qInfo() << "No translation available for" << languageSuffix;
        delete newTranslator;
    }

    if (m_QmlEngine != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        // This is a dynamic retranslation from the settings page.
        // We have to kick the QML engine into reloading our text.
        m_QmlEngine->retranslate();
#else
        // Unreachable below Qt 5.10 due to the check above
        Q_ASSERT(false);
#endif
    }
    else {
        // This is a translation from a non-QML context, which means
        // it is probably app startup. There's nothing to refresh.
    }

    return true;
}

QString StreamingPreferences::getSuffixFromLanguage(StreamingPreferences::Language lang)
{
    switch (lang)
    {
    case LANG_DE:
        return "de";
    case LANG_EN:
        return "en";
    case LANG_FR:
        return "fr";
    case LANG_ZH_CN:
        return "zh_CN";
    case LANG_NB_NO:
        return "nb_NO";
    case LANG_RU:
        return "ru";
    case LANG_ES:
        return "es";
    case LANG_JA:
        return "ja";
    case LANG_VI:
        return "vi";
    case LANG_TH:
        return "th";
    case LANG_KO:
        return "ko";
    case LANG_HU:
        return "hu";
    case LANG_NL:
        return "nl";
    case LANG_SV:
        return "sv";
    case LANG_TR:
        return "tr";
    case LANG_UK:
        return "uk";
    case LANG_ZH_TW:
        return "zh_TW";
    case LANG_PT:
        return "pt";
    case LANG_PT_BR:
        return "pt_BR";
    case LANG_EL:
        return "el";
    case LANG_IT:
        return "it";
    case LANG_HI:
        return "hi";
    case LANG_PL:
        return "pl";
    case LANG_CS:
        return "cs";
    case LANG_HE:
        return "he";
    case LANG_CKB:
        return "ckb";
    case LANG_LT:
        return "lt";
    case LANG_ET:
        return "et";
    case LANG_BG:
        return "bg";
    case LANG_EO:
        return "eo";
    case LANG_TA:
        return "ta";
    case LANG_AUTO:
    default:
        return QLocale::system().name();
    }
}

void StreamingPreferences::save()
{
    ProfileManager* prefs = ProfileManager::instance();

    prefs->setValue(SER_WIDTH, width);
    prefs->setValue(SER_HEIGHT, height);
    prefs->setValue(SER_FPS, fps);
    prefs->setValue(SER_BITRATE, bitrateKbps);
    prefs->setValue(SER_UNLOCK_BITRATE, unlockBitrate);
    prefs->setValue(SER_AUTOADJUSTBITRATE, autoAdjustBitrate);
    prefs->setValue(SER_VSYNC, enableVsync);
    prefs->setValue(SER_GAMEOPTS, gameOptimizations);
    prefs->setValue(SER_HOSTAUDIO, playAudioOnHost);
    prefs->setValue(SER_MULTICONT, multiController);
    prefs->setValue(SER_MDNS, enableMdns);
    prefs->setValue(SER_QUITAPPAFTER, quitAppAfter);
    prefs->setValue(SER_ABSMOUSEMODE, absoluteMouseMode);
    prefs->setValue(SER_ABSTOUCHMODE, absoluteTouchMode);
    prefs->setValue(SER_FRAMEPACING, framePacing);
    prefs->setValue(SER_CONNWARNINGS, connectionWarnings);
    prefs->setValue(SER_CONFWARNINGS, configurationWarnings);
    prefs->setValue(SER_RICHPRESENCE, richPresence);
    prefs->setValue(SER_GAMEPADMOUSE, gamepadMouse);
    prefs->setValue(SER_PACKETSIZE, packetSize);
    prefs->setValue(SER_DETECTNETBLOCKING, detectNetworkBlocking);
    prefs->setValue(SER_SHOWPERFOVERLAY, showPerformanceOverlay);
    prefs->setValue(SER_AUDIOCFG, static_cast<int>(audioConfig));
    prefs->setValue(SER_HDR, enableHdr);
    prefs->setValue(SER_YUV444, enableYUV444);
    prefs->setValue(SER_VIDEOCFG, static_cast<int>(videoCodecConfig));
    prefs->setValue(SER_VIDEODEC, static_cast<int>(videoDecoderSelection));
    prefs->setValue(SER_RENDERER, static_cast<int>(rendererSelection));
    prefs->setValue(SER_WINDOWMODE, static_cast<int>(windowMode));
    prefs->setValue(SER_UIDISPLAYMODE, static_cast<int>(uiDisplayMode));
    prefs->setValue(SER_LANGUAGE, static_cast<int>(language));
    prefs->setValue(SER_RENDERERBACKEND, static_cast<int>(rendererBackend));
    prefs->setValue(SER_DEFAULTVER, CURRENT_DEFAULT_VER);
    prefs->setValue(SER_SWAPMOUSEBUTTONS, swapMouseButtons);
    prefs->setValue(SER_MUTEONFOCUSLOSS, muteOnFocusLoss);
    prefs->setValue(SER_BACKGROUNDGAMEPAD, backgroundGamepad);
    prefs->setValue(SER_REVERSESCROLL, reverseScrollDirection);
    prefs->setValue(SER_SWAPFACEBUTTONS, swapFaceButtons);
    prefs->setValue(SER_CAPTURESYSKEYS, captureSysKeysMode);
    prefs->setValue(SER_KEEPAWAKE, keepAwake);
    
    // Artemis client-side streaming enhancements
    prefs->setValue(SER_VIRTUALDISPLAY, useVirtualDisplay);
    prefs->setValue(SER_FRACTIONALREFRESHRATE, enableFractionalRefreshRate);
    prefs->setValue(SER_CUSTOMREFRESHRATE, customRefreshRate);
    prefs->setValue(SER_RESOLUTIONSCALING, enableResolutionScaling);
    prefs->setValue(SER_RESOLUTIONSCALEFACTOR, resolutionScaleFactor);
    prefs->setValue(SER_MULTIDISPLAY, useMultiDisplay);

    // With a profile active every setValue() above landed in an in-memory map,
    // so this is what actually persists them. It also syncs QSettings when no
    // profile is active.
    prefs->save();
}

void StreamingPreferences::notifyAllPropertiesChanged()
{
    // Switching profiles replaces every member at once, and MEMBER properties
    // don't emit anything on their own. Walk the metaobject instead of hand
    // listing 40-odd signals that would drift the next time one is added.
    const QMetaObject* mo = metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); i++) {
        QMetaProperty property = mo->property(i);
        if (property.hasNotifySignal()) {
            property.notifySignal().invoke(this, Qt::DirectConnection);
        }
    }
}

int StreamingPreferences::getDefaultBitrate(int width, int height, int fps, bool yuv444)
{
    // Don't scale bitrate linearly beyond 60 FPS. It's definitely not a linear
    // bitrate increase for frame rate once we get to values that high.
    float frameRateFactor = (fps <= 60 ? fps : (qSqrt(fps / 60.f) * 60.f)) / 30.f;

    // TODO: Collect some empirical data to see if these defaults make sense.
    // We're just using the values that the Shield used, as we have for years.
    static const struct resTable {
        int pixels;
        int factor;
    } resTable[] {
        { 640 * 360, 1 },
        { 854 * 480, 2 },
        { 1280 * 720, 5 },
        { 1920 * 1080, 10 },
        { 2560 * 1440, 20 },
        { 3840 * 2160, 40 },
        { -1, -1 },
    };

    // Calculate the resolution factor by linear interpolation of the resolution table
    float resolutionFactor;
    int pixels = width * height;
    for (int i = 0;; i++) {
        if (pixels == resTable[i].pixels) {
            // We can bail immediately for exact matches
            resolutionFactor = resTable[i].factor;
            break;
        }
        else if (pixels < resTable[i].pixels) {
            if (i == 0) {
                // Never go below the lowest resolution entry
                resolutionFactor = resTable[i].factor;
            }
            else {
                // Interpolate between the entry greater than the chosen resolution (i) and the entry less than the chosen resolution (i-1)
                resolutionFactor = ((float)(pixels - resTable[i-1].pixels) / (resTable[i].pixels - resTable[i-1].pixels)) * (resTable[i].factor - resTable[i-1].factor) + resTable[i-1].factor;
            }
            break;
        }
        else if (resTable[i].pixels == -1) {
            // Never go above the highest resolution entry
            resolutionFactor = resTable[i-1].factor;
            break;
        }
    }

    if (yuv444) {
        // This is rough estimation based on the fact that 4:4:4 doubles the amount of raw YUV data compared to 4:2:0
        resolutionFactor *= 2;
    }

    return qRound(resolutionFactor * frameRateFactor) * 1000;
}
