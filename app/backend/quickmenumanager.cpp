#include "quickmenumanager.h"
#include "servercommandmanager.h"
#include "clipboardmanager.h"
#include "../streaming/session.h"
#include "settings/streamingpreferences.h"
#include "../streaming/panzoom.h"
#include "../streaming/input/input.h"
#ifdef LAV_TEST_BUILD
#include "../testctl/hiddenwindows.h"
#endif

#include <QQmlContext>
#include <QQmlEngine>
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QWindow>
#include <QPointer>
#include <QThreadPool>

#include "nvhttp.h"
#include <QUrl>
#include <QDebug>
#include <QTimer>

QuickMenuManager::QuickMenuManager(QObject *parent)
    : QObject(parent)
    , m_isVisible(false)
    , m_window(nullptr)
    , m_quickView(nullptr)
    , m_quickMenuItem(nullptr)
    , m_serverCommandManager(nullptr)
    , m_clipboardManager(nullptr)
    , m_isFullscreen(false)
    , m_isMouseCaptured(false)
    , m_isKeyboardCaptured(false)
    , m_isStatsVisible(false)
{
}

QuickMenuManager::~QuickMenuManager()
{
    if (m_quickView) {
        delete m_quickView;
    }
}

bool QuickMenuManager::isFullscreen() const
{
    return m_isFullscreen;
}

bool QuickMenuManager::isMouseCaptured() const
{
    return m_isMouseCaptured;
}

bool QuickMenuManager::isKeyboardCaptured() const
{
    return m_isKeyboardCaptured;
}

bool QuickMenuManager::isStatsVisible() const
{
    // Read from the overlay: the Ctrl+Alt+Shift+S shortcut toggles it without the menu
    if (Session::get()) {
        return Session::get()->getOverlayManager().isOverlayEnabled(Overlay::OverlayDebug);
    }
    return m_isStatsVisible;
}

void QuickMenuManager::setVisible(bool visible)
{
    if (m_isVisible == visible) {
        return;
    }
    
    m_isVisible = visible;
    
    if (visible) {
        refreshPlayniteGame();
        if (m_playniteTimer == nullptr) {
            m_playniteTimer = new QTimer(this);
            m_playniteTimer->setInterval(2000);
            connect(m_playniteTimer, &QTimer::timeout, this, &QuickMenuManager::refreshPlayniteGame);
        }
        m_playniteTimer->start();
        if (m_serverCommandManager) {
            m_serverCommandManager->refreshCommands();
        }
        createQuickView();
    } else {
        if (m_playniteTimer) {
            m_playniteTimer->stop();
        }
        if (m_quickView) {
            m_quickView->hide();
        }
    }
    
    emit visibilityChanged();
}

void QuickMenuManager::toggle()
{
    setVisible(!m_isVisible);
}

void QuickMenuManager::refreshPlayniteGame()
{
    if (m_computer == nullptr || m_playniteQueryRunning) {
        return;
    }

    // Host polling is stopped while streaming: ask the host now, off the UI thread (one question at a time).
    m_playniteQueryRunning = true;
    const NvHttpTarget target = NvHttpTarget::of(m_computer);
    QPointer<QuickMenuManager> self(this);
    QThreadPool::globalInstance()->start([target, self]() {
        QString name;
        bool ok = true;
        try {
            NvHTTP http(target);
            name = NvHTTP::getXmlString(http.getServerInfo(NvHTTP::NVLL_NONE, true), "PlayniteGameName");
        } catch (...) {
            ok = false;
        }
        QMetaObject::invokeMethod(qApp, [self, name, ok]() {
            if (self) {
                self->m_playniteQueryRunning = false;
            }
            if (ok && self && self->m_playniteGameName != name) {
                self->m_playniteGameName = name;
                emit self->playniteGameChanged();
            }
        }, Qt::QueuedConnection);
    });
}

void QuickMenuManager::quitPlayniteGame()
{
    if (m_computer == nullptr) {
        return;
    }

    const NvHttpTarget target = NvHttpTarget::of(m_computer);
    QPointer<QuickMenuManager> self(this);
    QThreadPool::globalInstance()->start([target, self]() {
        bool ok = true;
        try {
            NvHTTP http(target);
            QString reply = http.openConnectionToString(http.m_BaseUrlHttps, "playnite-quit-game", nullptr, 5000);
            NvHTTP::verifyResponseStatus(reply);
        } catch (...) {
            ok = false;
        }
        QMetaObject::invokeMethod(qApp, [self, ok]() {
            if (self) {
                self->showToast(ok ? tr("Fermeture du jeu, retour à Playnite…")
                                   : tr("Impossible de fermer le jeu dans Playnite"));
            }
        }, Qt::QueuedConnection);
    });
}

void QuickMenuManager::show()
{
    setVisible(true);
}

void QuickMenuManager::hide()
{
    setVisible(false);
}

void QuickMenuManager::executeAction(const QString &action)
{
    qDebug() << "QuickMenuManager: Executing action:" << action;
    
    if (action == "disconnect") {
        disconnect();
    } else if (action == "quit") {
        quit();
    } else if (action == "clipboard_upload") {
        uploadClipboard();
    } else if (action == "clipboard_fetch") {
        fetchClipboard();
    } else if (action == "toggle_stats") {
        toggleStats();
    } else if (action == "toggle_mouse") {
        toggleMouseCapture();
    } else if (action == "toggle_keyboard") {
        toggleKeyboardCapture();
    } else if (action == "toggle_fullscreen") {
        toggleFullscreen();
    } else if (action == "toggle_mouse_mode") {
        // « Plus › Comportement de la souris »: Ctrl+Alt+Shift+M
        if (Session::get() && Session::get()->getInputHandler()) {
            Session::get()->getInputHandler()->toggleMouseMode();
        }
    } else if (action == "minimize") {
        // « Plus › Réduire la fenêtre »: Ctrl+Alt+Shift+D
        if (Session::get() && Session::get()->getInputHandler()) {
            Session::get()->getInputHandler()->minimizeStreamWindow();
        }
    } else if (action == "toggle_microphone") {
        if (Session::get()) {
            Session::get()->toggleMicrophoneMute();
        }
    } else if (action == "toggle_touch_mode") {
        // Like Android's « Passer en trackpad / multitouch »: for this stream, and kept for the
        // next ones with « Retenir le mode tactile choisi en jeu » (Game.switchTouchMode)
        if (Session::get() && Session::get()->getInputHandler()) {
            SdlInputHandler* input = Session::get()->getInputHandler();
            input->setAbsoluteTouchMode(!input->isAbsoluteTouchMode());
            StreamingPreferences* prefs = StreamingPreferences::get();
            if (prefs->rememberTouchMode) {
                // This setting only: save() would also write what the command line overrode for this
                // launch (--resolution, --fps...), as if the user had chosen it in Réglages
                prefs->saveAbsoluteTouchMode(input->isAbsoluteTouchMode());
            }
        }
    }
}

void QuickMenuManager::showToast(const QString &message)
{
    // Over the stream, like « Micro coupé »: the stream window may be in exclusive full screen,
    // where another window on top would not show (or would knock it out of full screen)
    if (Session::get()) {
        Session::get()->showNotification(message);
    }
}

void QuickMenuManager::disconnect()
{
    qDebug() << "QuickMenuManager: Disconnect requested";
    emit disconnectRequested();

    // Only a stream that runs can be ended from the menu: a quit request left over would end the
    // next stream at once, or the whole program (SdlGamepadKeyNavigation reads SDL_QUIT)
    Session* session = Session::get();
    if (session == nullptr || !session->isStreamLoopRunning()) {
        return;
    }
    session->disconnectFromMenu();
}

void QuickMenuManager::quit()
{
    qDebug() << "QuickMenuManager: Quit requested";
    emit quitRequested();

    // « Quitter la session »: quit the app on the host, end the stream and go back to the
    // client's home, as Android does. Not setShouldExit(): that also exits the whole program
    // (the quit-and-exit key combo, SIGTERM). A stream launched from the command line or an
    // art:// link still exits once it ends (StreamSegue.quitAfter).
    Session* session = Session::get();
    if (session == nullptr || !session->isStreamLoopRunning()) {
        return;
    }
    session->setQuitHostApp();
    session->requestStreamEnd();
}

void QuickMenuManager::uploadClipboard()
{
    qDebug() << "QuickMenuManager: Clipboard upload requested";
    emit clipboardUploadRequested();
    
    if (m_clipboardManager) {
        m_clipboardManager->sendClipboard();
    }
}

void QuickMenuManager::fetchClipboard()
{
    qDebug() << "QuickMenuManager: Clipboard fetch requested";
    emit clipboardFetchRequested();
    
    if (m_clipboardManager) {
        m_clipboardManager->getClipboard();
    }
}

void QuickMenuManager::toggleStats()
{
    qDebug() << "QuickMenuManager: Stats toggle requested";
    emit statsToggleRequested();
    
    // Toggle debug overlay (performance stats)
    if (Session::get()) {
        auto& overlayManager = Session::get()->getOverlayManager();
        bool currentState = overlayManager.isOverlayEnabled(Overlay::OverlayDebug);
        overlayManager.setOverlayState(Overlay::OverlayDebug, !currentState);
        
        m_isStatsVisible = !currentState;
        emit statsVisibilityChanged();
    }
}

// The three entries below did nothing until 21.8.0-dev (sendKeyCombo() is an empty placeholder):
// they now run the same code as the Ctrl+Alt+Shift shortcuts Z, K and X.
void QuickMenuManager::toggleMouseCapture()
{
    qDebug() << "QuickMenuManager: Mouse capture toggle requested";
    emit mouseCaptureToggleRequested();

    if (Session::get() && Session::get()->getInputHandler()) {
        Session::get()->getInputHandler()->toggleInputCapture();
    }
}

void QuickMenuManager::toggleKeyboardCapture()
{
    qDebug() << "QuickMenuManager: Keyboard capture toggle requested";
    emit keyboardCaptureToggleRequested();

    if (Session::get() && Session::get()->getInputHandler()) {
        Session::get()->getInputHandler()->toggleSystemKeyCapture();
    }
}

void QuickMenuManager::toggleFullscreen()
{
    qDebug() << "QuickMenuManager: Fullscreen toggle requested";
    emit fullscreenToggleRequested();

    if (Session::get() && Session::get()->getInputHandler()) {
        Session::get()->getInputHandler()->toggleFullScreenMode();
    }
}

bool QuickMenuManager::microphoneActive() const
{
    return Session::get() && Session::get()->isMicrophoneActive();
}

bool QuickMenuManager::microphoneMuted() const
{
    return Session::get() && Session::get()->isMicrophoneMuted();
}

bool QuickMenuManager::rememberTouchMode() const
{
    return StreamingPreferences::get()->rememberTouchMode;
}

void QuickMenuManager::setStreamInfo(const QString& hostName, const QString& appTitle, const QString& appKind)
{
    m_hostName = hostName;
    m_appTitle = appTitle;
    m_appKind = appKind;
    emit streamInfoChanged();
}

void QuickMenuManager::toggleZoom()
{
    // The menu runs on the main thread, like the SDL loop that reads PanZoom
    if (PanZoom::isActive()) {
        PanZoom::reset();
    }
    else {
        PanZoom::zoomBy(2.0f);
    }
}

bool QuickMenuManager::streamFullScreen() const
{
    return Session::get() && Session::get()->isStreamWindowFullScreen();
}

bool QuickMenuManager::desktopMouseMode() const
{
    return Session::get() && Session::get()->getInputHandler() &&
           Session::get()->getInputHandler()->isAbsoluteMouseMode();
}

int QuickMenuManager::zoomPercent() const
{
    return qRound(PanZoom::scale() * 100.0f);
}

bool QuickMenuManager::trackpadTouchMode() const
{
    return Session::get() && Session::get()->getInputHandler() &&
           !Session::get()->getInputHandler()->isAbsoluteTouchMode();
}

void QuickMenuManager::setServerCommandManager(ServerCommandManager *manager)
{
    if (m_serverCommandManager) {
        QObject::disconnect(m_serverCommandManager, nullptr, this, nullptr);
    }
    
    m_serverCommandManager = manager;

    emit serverCommandsChanged();
}

void QuickMenuManager::setClipboardManager(ClipboardManager *manager)
{
    m_clipboardManager = manager;
}

void QuickMenuManager::setWindow(QWindow *window)
{
    m_window = window;
}

void QuickMenuManager::setStreamWindowRect(const QRect& rect)
{
    m_streamWindowRect = rect;
    if (m_quickView && m_isVisible) {
        placeQuickView();
    }
}

void QuickMenuManager::placeQuickView()
{
    // Over the whole stream window (prototype V8: the menu is an overlay of the stream, its scrim and
    // tiles drawn over the picture), in Qt's logical coordinates (Session converts the SDL window's
    // rectangle: SDL counts physical pixels on Windows). A transparent window: the stream shows
    // through the scrim.
    if (m_streamWindowRect.isValid()) {
        m_quickView->setGeometry(m_streamWindowRect);
    }
    else {
        m_quickView->setGeometry(QRect(QPoint(100, 100), QSize(1280, 720)));
    }
}

void QuickMenuManager::onServerCommandsChanged()
{
    emit serverCommandsChanged();
}

void QuickMenuManager::onFullscreenChanged()
{
    emit fullscreenChanged();
}

void QuickMenuManager::onMouseCaptureChanged()
{
    emit mouseCaptureChanged();
}

void QuickMenuManager::onKeyboardCaptureChanged()
{
    emit keyboardCaptureChanged();
}

void QuickMenuManager::onStatsVisibilityChanged()
{
    emit statsVisibilityChanged();
}

void QuickMenuManager::createQuickView()
{
    qDebug() << "QuickMenuManager::createQuickView() called";
    
    if (!m_quickView) {
        qDebug() << "Creating new QQuickView";
        m_quickView = new QQuickView();
        m_quickView->setResizeMode(QQuickView::SizeRootObjectToView);
        
        // Set up the QML context
        QQmlContext *context = m_quickView->rootContext();
        context->setContextProperty("quickMenuManager", this);
        
        // Load the QML file
        qDebug() << "Loading QML from: qrc:/gui/QuickMenu.qml";
        m_quickView->setSource(QUrl("qrc:/gui/QuickMenu.qml"));
        
        qDebug() << "QML loading status:" << m_quickView->status();
        if (m_quickView->status() == QQuickView::Error) {
            qDebug() << "QuickMenuManager: Error loading QML:" << m_quickView->errors();
            return;
        } else if (m_quickView->status() == QQuickView::Ready) {
            qDebug() << "QML loaded successfully";
        } else {
            qDebug() << "QML loading in progress, status:" << m_quickView->status();
        }
        
        m_quickMenuItem = m_quickView->rootObject();
        qDebug() << "Root object:" << m_quickMenuItem;
        
        if (m_quickMenuItem) {
            qDebug() << "Root object created successfully";
            // Make sure the root object is visible
            m_quickMenuItem->setProperty("visible", true);
            qDebug() << "Root object set to visible";
        } else {
            qDebug() << "Failed to create root object";
        }
        
        // Make it a popup overlay
#ifdef LAV_TEST_BUILD
        // Test build: never topmost while the windows are hidden (testctl/hiddenwindows.h); on
        // top again with LAV_TEST_VISIBLE=1, to be seen over the stream
        m_quickView->setFlags((TestWindows::active() ? Qt::WindowFlags() : Qt::WindowStaysOnTopHint) |
                              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
#else
        m_quickView->setFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
#endif
        
        // Set transparent background
        m_quickView->setColor(QColor(Qt::transparent));
    }

    if (m_quickView) {
        // Placed at each opening: the stream window may have moved or changed size since
        placeQuickView();
        m_quickView->show();
#ifndef LAV_TEST_BUILD // test build: the menu never takes the focus
        m_quickView->raise();
        m_quickView->requestActivate();
#endif
    }
}

