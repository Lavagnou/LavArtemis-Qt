#pragma once

#include <QObject>
#include <QRect>
#include <QColor>

class QWindow;

#include "SDL_compat.h"

class SystemProperties : public QObject
{
    Q_OBJECT

    friend class SystemPropertyQueryThread;

public:
    SystemProperties();
    ~SystemProperties();

    // Static properties queried synchronously during the constructor
    Q_PROPERTY(bool isRunningWayland MEMBER isRunningWayland CONSTANT)
    Q_PROPERTY(bool isRunningXWayland MEMBER isRunningXWayland CONSTANT)
    Q_PROPERTY(bool isWow64 MEMBER isWow64 CONSTANT)
    Q_PROPERTY(bool isDarwin MEMBER isDarwin CONSTANT)
    Q_PROPERTY(QString friendlyNativeArchName MEMBER friendlyNativeArchName CONSTANT)
    Q_PROPERTY(bool hasDesktopEnvironment MEMBER hasDesktopEnvironment CONSTANT)
    // A touchscreen on this PC (read at startup): Réglages offers the touch-trackpad option only then
    Q_PROPERTY(bool hasTouchScreen MEMBER hasTouchScreen CONSTANT)
    Q_PROPERTY(bool hasBrowser MEMBER hasBrowser CONSTANT)
    Q_PROPERTY(bool hasDiscordIntegration MEMBER hasDiscordIntegration CONSTANT)
    Q_PROPERTY(bool hasPyroWave MEMBER hasPyroWave CONSTANT)
    // LavArtemis Dev build (a "-dev." version, see app.pro)
    Q_PROPERTY(bool isDevChannel MEMBER isDevChannel CONSTANT)
    Q_PROPERTY(bool usesMaterial3Theme MEMBER usesMaterial3Theme CONSTANT)
    Q_PROPERTY(QString versionString MEMBER versionString CONSTANT)

    // Properties queried asynchronously (startAsyncLoad() must be called!)
    Q_PROPERTY(bool hasHardwareAcceleration MEMBER hasHardwareAcceleration NOTIFY hasHardwareAccelerationChanged)
    Q_PROPERTY(bool rendererAlwaysFullScreen MEMBER rendererAlwaysFullScreen NOTIFY rendererAlwaysFullScreenChanged)
    Q_PROPERTY(QString unmappedGamepads MEMBER unmappedGamepads NOTIFY unmappedGamepadsChanged)
    Q_PROPERTY(QSize maximumResolution MEMBER maximumResolution NOTIFY maximumResolutionChanged)
    Q_PROPERTY(bool supportsHdr MEMBER supportsHdr NOTIFY supportsHdrChanged)

    // Either startAsyncLoad()+waitForAsyncLoad() or refreshDisplays() must be invoked first
    Q_INVOKABLE QRect getNativeResolution(int displayIndex);
    Q_INVOKABLE QRect getSafeAreaResolution(int displayIndex);
    Q_INVOKABLE int getRefreshRate(int displayIndex);

    // « 25 min jouées » / « 3 h jouées » / « Jamais lancé », and the short « 25 min » / « 3 h » / « — »
    // (backend/playtimetext, tests/playtime)
    Q_INVOKABLE QString playtimeText(double seconds) const;
    Q_INVOKABLE QString playtimeShort(double seconds) const;
    // The library's count: « ATLAS-PC · 265 jeux » / « 12 affichés sur 265 » / « aucun jeu », and on
    // « Masqués »: « 3 jeux masqués » (backend/librarycounttext, tests/librarycount)
    Q_INVOKABLE QString libraryCountText(const QString& hostName, int shown, int visible, bool allFilter,
                                         bool hiddenFilter = false) const;

    Q_INVOKABLE void startAsyncLoad();
    Q_INVOKABLE void waitForAsyncLoad();
    Q_INVOKABLE void refreshDisplays();

    // Colours the native title bar of a window (Windows 11, ignored elsewhere): the redesigned
    // interface gives it the theme's background and text so the system accent bar disappears.
    // useDefault gives the bar back to Windows (legacy interface).
    Q_INVOKABLE void setWindowFrameColors(QWindow* window, const QColor& caption, const QColor& text,
                                          bool dark, bool useDefault);

    // The same colours for a native window created outside Qt (the SDL stream window).
    static void applyWindowFrameColors(void* nativeWindowHandle);

signals:
    void unmappedGamepadsChanged();
    void hasHardwareAccelerationChanged();
    void rendererAlwaysFullScreenChanged();
    void maximumResolutionChanged();
    void supportsHdrChanged();

private slots:
    void updateDecoderProperties(bool hasHardwareAcceleration, bool rendererAlwaysFullScreen, QSize maximumResolution, bool supportsHdr);

private:
    // Last colours set by setWindowFrameColors(), reused by applyWindowFrameColors()
    static QColor s_FrameCaption;
    static QColor s_FrameText;
    static bool s_FrameDark;
    static bool s_FrameDefault;

    QThread* systemPropertyQueryThread = nullptr;
    SDL_Window* testWindow = nullptr;

    // Properties set by the constructor
    bool isRunningWayland;
    bool isRunningXWayland;
    bool isWow64;
    QString friendlyNativeArchName;
    bool hasDesktopEnvironment;
    bool hasTouchScreen;
    bool hasBrowser;
    bool hasDiscordIntegration;
    bool hasPyroWave;
    bool isDevChannel;
    QString versionString;
    bool usesMaterial3Theme;
    bool isDarwin;

    // Properties only set if startAsyncLoad() is called
    bool hasHardwareAcceleration;
    bool rendererAlwaysFullScreen;
    QSize maximumResolution;
    bool supportsHdr;
    QString unmappedGamepads;

    // Properties set by refreshDisplays()
    QList<QRect> monitorNativeResolutions;
    QList<QRect> monitorSafeAreaResolutions;
    QList<int> monitorRefreshRates;
};

