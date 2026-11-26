#include "systemproperties.h"
#include "utils.h"
#include "playtimetext.h"
#include "librarycounttext.h"

#include <QGuiApplication>
#include <QInputDevice>
#include <QLibraryInfo>
#include <QFile>
#include <QProcess>

#include "streaming/session.h"
#include "streaming/streamutils.h"

#include <QWindow>

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#endif

QColor SystemProperties::s_FrameCaption;
QColor SystemProperties::s_FrameText;
bool SystemProperties::s_FrameDark = false;
bool SystemProperties::s_FrameDefault = true;

static void applyFrameColors(void* handle, const QColor& caption, const QColor& text, bool dark, bool useDefault)
{
#ifdef Q_OS_WIN32
    HWND hwnd = reinterpret_cast<HWND>(handle);
    if (hwnd == nullptr) {
        return;
    }

    // DWMWINDOWATTRIBUTE values, spelled out for SDKs older than Windows 11. On Windows 10 the
    // colour attributes fail and the bar keeps its system look: nothing else depends on them.
    const DWORD useImmersiveDarkMode = 20, borderColor = 34, captionColor = 35, textColor = 36;
    const COLORREF systemDefault = 0xFFFFFFFF; // DWMWA_COLOR_DEFAULT

    if (!useDefault) {
        BOOL darkMode = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, useImmersiveDarkMode, &darkMode, sizeof(darkMode));
    }
    COLORREF cap = useDefault ? systemDefault : RGB(caption.red(), caption.green(), caption.blue());
    COLORREF txt = useDefault ? systemDefault : RGB(text.red(), text.green(), text.blue());
    DwmSetWindowAttribute(hwnd, captionColor, &cap, sizeof(cap));
    DwmSetWindowAttribute(hwnd, borderColor, &cap, sizeof(cap));
    DwmSetWindowAttribute(hwnd, textColor, &txt, sizeof(txt));

    // The redesigned header already shows the logo and the name: the bar stops drawing them (the
    // window keeps its title and icon for the taskbar and Alt+Tab). The legacy interface gets them back.
    WTA_OPTIONS options;
    options.dwMask = WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON;
    options.dwFlags = useDefault ? 0 : (WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON);
    SetWindowThemeAttribute(hwnd, WTA_NONCLIENT, &options, sizeof(options));
#else
    Q_UNUSED(handle);
    Q_UNUSED(caption);
    Q_UNUSED(text);
    Q_UNUSED(dark);
    Q_UNUSED(useDefault);
#endif
}

void SystemProperties::setWindowFrameColors(QWindow* window, const QColor& caption, const QColor& text,
                                            bool dark, bool useDefault)
{
    s_FrameCaption = caption;
    s_FrameText = text;
    s_FrameDark = dark;
    s_FrameDefault = useDefault;
    if (window != nullptr) {
        applyFrameColors(reinterpret_cast<void*>(window->winId()), caption, text, dark, useDefault);
    }
}

void SystemProperties::applyWindowFrameColors(void* nativeWindowHandle)
{
    if (!s_FrameDefault) {
        applyFrameColors(nativeWindowHandle, s_FrameCaption, s_FrameText, s_FrameDark, false);
    }
}

class SystemPropertyQueryThread : public QThread
{
public:
    SystemPropertyQueryThread(SystemProperties* properties)
        : QThread(properties), m_Properties(properties)
    {
        setObjectName("System Properties Async Query Thread");
    }

private:
    void run() override
    {
        bool hasHardwareAcceleration;
        bool rendererAlwaysFullScreen;
        bool supportsHdr;
        QSize maximumResolution;

        Session::getDecoderInfo(m_Properties->testWindow, hasHardwareAcceleration, rendererAlwaysFullScreen, supportsHdr, maximumResolution);

        // Propagate the decoder properties to the SystemProperties singleton and emit any change signals on the main thread
        QMetaObject::invokeMethod(m_Properties, "updateDecoderProperties",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, hasHardwareAcceleration),
                                  Q_ARG(bool, rendererAlwaysFullScreen),
                                  Q_ARG(QSize, maximumResolution),
                                  Q_ARG(bool, supportsHdr));
    }

private:
    SystemProperties* m_Properties;
};

SystemProperties::SystemProperties()
{
    versionString = QString(VERSION_STR);
    hasDesktopEnvironment = WMUtils::isRunningDesktopEnvironment();

    hasTouchScreen = false;
#ifdef Q_OS_WIN32
    // Digitizer touch points, the way Qt itself decides to register a touch device
    hasTouchScreen = GetSystemMetrics(SM_MAXIMUMTOUCHES) > 0;
#endif
    for (const QInputDevice* device : QInputDevice::devices()) {
        if (device->type() == QInputDevice::DeviceType::TouchScreen) {
            hasTouchScreen = true;
        }
    }
#ifdef LAV_TEST_BUILD
    // Test build: no real touch ever reaches the stream (testinput.h), so this PC's screen does not
    // count. LAV_TEST_TOUCHSCREEN=1 (set by the bench) fakes one: the touch rows of Réglages and the
    // touch-mode entry of the in-game menu show, the same on every PC.
    hasTouchScreen = qgetenv("LAV_TEST_TOUCHSCREEN") == "1";
#endif
    isRunningWayland = WMUtils::isRunningWayland();
    isRunningXWayland = isRunningWayland && QGuiApplication::platformName() == "xcb";
    usesMaterial3Theme = QLibraryInfo::version() >= QVersionNumber(6, 5, 0);

#ifdef Q_OS_DARWIN
    isDarwin = true;
#else
    isDarwin = false;
#endif

    QString nativeArch = QSysInfo::currentCpuArchitecture();

#ifdef Q_OS_WIN32
    {
        USHORT processArch, machineArch;

        // Use IsWow64Process2() because it doesn't lie on ARM64
        if (IsWow64Process2(GetCurrentProcess(), &processArch, &machineArch)) {
            switch (machineArch) {
            case IMAGE_FILE_MACHINE_I386:
                nativeArch = "i386";
                break;
            case IMAGE_FILE_MACHINE_AMD64:
                nativeArch = "x86_64";
                break;
            case IMAGE_FILE_MACHINE_ARM64:
                nativeArch = "arm64";
                break;
            }
        }

        isWow64 = nativeArch != QSysInfo::buildCpuArchitecture();
    }
#else
    isWow64 = false;
#endif

    if (nativeArch == "i386") {
        friendlyNativeArchName = "x86";
    }
    else if (nativeArch == "x86_64") {
        friendlyNativeArchName = "x64";
    }
    else {
        friendlyNativeArchName = nativeArch.toUpper();
    }

    // Assume we can probably launch a browser if we're in a GUI environment
    hasBrowser = hasDesktopEnvironment;

#ifdef HAVE_DISCORD
    hasDiscordIntegration = true;
#else
    hasDiscordIntegration = false;
#endif

    // PyroWave has a D3D11 path on Windows and a Vulkan path on Linux, and is
    // only built for x86_64 (see app.pro).
#ifdef HAVE_PYROWAVE
    hasPyroWave = true;
#else
    hasPyroWave = false;
#endif

#ifdef LAV_DEV_CHANNEL
    isDevChannel = true;
#else
    isDevChannel = false;
#endif

    // These will be queried asynchronously to avoid blocking the UI
    hasHardwareAcceleration = true;
    rendererAlwaysFullScreen = false;
    supportsHdr = true;
    maximumResolution = QSize(0, 0);
}

SystemProperties::~SystemProperties()
{
    waitForAsyncLoad();
}

void SystemProperties::updateDecoderProperties(bool hasHardwareAcceleration, bool rendererAlwaysFullScreen, QSize maximumResolution, bool supportsHdr)
{
    SDL_assert(testWindow);

    // Escape hatches for sandboxed builds (Flatpak) and for debugging, where the
    // probe can under-report what the runtime actually supports. These used to live
    // in startAsyncLoad() before the probe became asynchronous upstream; they have to
    // be applied here now, on the probed values, or they would be overwritten.
    if (qgetenv("LAVARTEMIS_FORCE_HW_ACCEL") == "1" || qgetenv("ARTEMIS_FORCE_HW_ACCEL") == "1") {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Forcing hardware acceleration on via environment variable (probed: %s)",
                    hasHardwareAcceleration ? "ENABLED" : "DISABLED");
        hasHardwareAcceleration = true;
    }

    if (qgetenv("FORCE_HDR_SUPPORT") == "1") {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Forcing HDR support on via environment variable (probed: %s)",
                    supportsHdr ? "ENABLED" : "DISABLED");
        supportsHdr = true;
    }

    if (qgetenv("HDR_DEBUG") == "1") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "HDR debug: hardware acceleration=%s, renderer fullscreen=%s, HDR support=%s",
                    hasHardwareAcceleration ? "YES" : "NO",
                    rendererAlwaysFullScreen ? "YES" : "NO",
                    supportsHdr ? "YES" : "NO");
    }

    if (hasHardwareAcceleration != this->hasHardwareAcceleration) {
        this->hasHardwareAcceleration = hasHardwareAcceleration;
        emit hasHardwareAccelerationChanged();
    }

    if (rendererAlwaysFullScreen != this->rendererAlwaysFullScreen) {
        this->rendererAlwaysFullScreen = rendererAlwaysFullScreen;
        emit rendererAlwaysFullScreenChanged();
    }

    if (maximumResolution != this->maximumResolution) {
        this->maximumResolution = maximumResolution;
        emit maximumResolutionChanged();
    }

    if (supportsHdr != this->supportsHdr) {
        this->supportsHdr = supportsHdr;
        emit supportsHdrChanged();
    }

    SDL_DestroyWindow(testWindow);
    testWindow = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

QRect SystemProperties::getNativeResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorNativeResolutions.value(displayIndex);
}

QRect SystemProperties::getSafeAreaResolution(int displayIndex)
{
    // Returns default constructed QRect if out of bounds
    return monitorSafeAreaResolutions.value(displayIndex);
}

int SystemProperties::getRefreshRate(int displayIndex)
{
    // Returns 0 if out of bounds
    return monitorRefreshRates.value(displayIndex);
}

QString SystemProperties::playtimeText(double seconds) const
{
    return PlaytimeText::played(static_cast<qint64>(seconds));
}

QString SystemProperties::playtimeShort(double seconds) const
{
    return PlaytimeText::shortForm(static_cast<qint64>(seconds));
}

QString SystemProperties::libraryCountText(const QString& hostName, int shown, int visible, bool allFilter) const
{
    return LibraryCountText::line(hostName, shown, visible, allFilter);
}

void SystemProperties::startAsyncLoad()
{
    if (systemPropertyQueryThread) {
        // Already started/completed
        return;
    }

    // This isn't actually asynchronous (due to the need to synchronize with
    // SdlGamepadKeyNavigation), but we don't query it in the constructor
    // because it's expensive.
    unmappedGamepads = SdlInputHandler::getUnmappedGamepads();
    if (!unmappedGamepads.isEmpty()) {
        emit unmappedGamepadsChanged();
    }

    // We initialize the video subsystem and test window on the main thread
    // because some platforms (macOS) do not support window creation on
    // non-main threads.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    testWindow = StreamUtils::createTestWindow();
    if (!testWindow) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create window for hardware decode test: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // Update display related attributes (max FPS, native resolution, etc).
    //
    // NB: SDL3 will forcefully refresh displays when a window is created,
    // so we place this after the window creation to ensure we don't pay
    // the penalty for mode enumeration twice.
    refreshDisplays();

    systemPropertyQueryThread = new SystemPropertyQueryThread(this);
    systemPropertyQueryThread->start();
}

void SystemProperties::waitForAsyncLoad()
{
    if (systemPropertyQueryThread) {
        systemPropertyQueryThread->wait();
    }
}

void SystemProperties::refreshDisplays()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return;
    }

    monitorNativeResolutions.clear();
    monitorSafeAreaResolutions.clear();
    monitorRefreshRates.clear();

    SDL_DisplayMode bestMode;
    for (int displayIndex = 0; displayIndex < SDL_GetNumVideoDisplays(); displayIndex++) {
        SDL_DisplayMode desktopMode;
        SDL_Rect safeArea;

        if (StreamUtils::getNativeDesktopMode(displayIndex, &desktopMode, &safeArea)) {
            if (desktopMode.w <= 8192 && desktopMode.h <= 8192) {
                // Keep these lists compact because their QML consumers iterate until
                // the first empty entry. Inserting by SDL display index is invalid if
                // an earlier display was skipped (for example, a >8K virtual display).
                monitorNativeResolutions.append(QRect(0, 0, desktopMode.w, desktopMode.h));
                monitorSafeAreaResolutions.append(QRect(0, 0, safeArea.w, safeArea.h));
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Skipping resolution over 8K: %dx%d",
                            desktopMode.w, desktopMode.h);
            }

            // Start at desktop mode and work our way up
            bestMode = desktopMode;
            int numDisplayModes = SDL_GetNumDisplayModes(displayIndex);
            for (int i = 0; i < numDisplayModes; i++) {
                SDL_DisplayMode mode;
                if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
                    if (mode.w == desktopMode.w && mode.h == desktopMode.h) {
                        if (mode.refresh_rate > bestMode.refresh_rate) {
                            bestMode = mode;
                        }
                    }
                }
            }

            // Try to normalize values around our our standard refresh rates.
            // Some displays/OSes report values that are slightly off.
            if (bestMode.refresh_rate >= 58 && bestMode.refresh_rate <= 62) {
                monitorRefreshRates.append(60);
            }
            else if (bestMode.refresh_rate >= 28 && bestMode.refresh_rate <= 32) {
                monitorRefreshRates.append(30);
            }
            else {
                monitorRefreshRates.append(bestMode.refresh_rate);
            }
        }
    }
}

