#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QWindow>
#include <QQuickItem>
#include <QQuickView>

class NvComputer;
class NvHTTP;
#include "backend/servercommandmanager.h"
class ClipboardManager;

/**
 * @brief Manages the Quick Menu overlay system
 * 
 * This class handles the display and interaction of the Quick Menu overlay
 * that provides easy access to common streaming functions and server commands.
 * It integrates with the existing overlay system and provides a modern QML-based
 * interface for touchscreen and keyboard navigation.
 */
class QuickMenuManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isVisible READ isVisible WRITE setVisible NOTIFY visibilityChanged)
    Q_PROPERTY(bool hasServerCommands READ hasServerCommands NOTIFY serverCommandsChanged)
    Q_PROPERTY(bool isFullscreen READ isFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool isMouseCaptured READ isMouseCaptured NOTIFY mouseCaptureChanged)
    Q_PROPERTY(bool isKeyboardCaptured READ isKeyboardCaptured NOTIFY keyboardCaptureChanged)
    Q_PROPERTY(bool isStatsVisible READ isStatsVisible NOTIFY statsVisibilityChanged)
    Q_PROPERTY(ServerCommandManager* serverCommandManager READ serverCommandManager CONSTANT)
    // LavApollo Link: the game running inside the host's Playnite (empty if none), refreshed
    // each time the menu opens; it offers "Quitter <jeu>", back to Playnite.
    Q_PROPERTY(QString playniteGameName READ playniteGameName NOTIFY playniteGameChanged)
    QML_ELEMENT

public:
    explicit QuickMenuManager(QObject *parent = nullptr);
    ~QuickMenuManager();

    // Property getters
    bool isVisible() const { return m_isVisible; }
    bool hasServerCommands() const;
    bool isFullscreen() const;
    bool isMouseCaptured() const;
    bool isKeyboardCaptured() const;
    bool isStatsVisible() const;

    // Menu management
    Q_INVOKABLE void setVisible(bool visible);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();

    // Action handlers
    Q_INVOKABLE void executeAction(const QString &action);
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void quit();
    Q_INVOKABLE void executeServerCommand(const QString &command);
    Q_INVOKABLE void showToast(const QString &message);
    Q_INVOKABLE void uploadClipboard();
    Q_INVOKABLE void fetchClipboard();
    Q_INVOKABLE void toggleStats();
    Q_INVOKABLE void toggleMouseCapture();
    Q_INVOKABLE void toggleKeyboardCapture();
    Q_INVOKABLE void toggleFullscreen();

    // "Quitter le jeu": closes the game inside Playnite, the stream goes back to Playnite
    Q_INVOKABLE void quitPlayniteGame();

    // State the menu reads each time it opens (main thread, like the session)
    Q_INVOKABLE bool microphoneActive() const;
    Q_INVOKABLE bool microphoneMuted() const;
    Q_INVOKABLE bool trackpadTouchMode() const;
    // « Retenir le mode tactile choisi en jeu »: the switch is kept for the next streams
    Q_INVOKABLE bool rememberTouchMode() const;
    QString playniteGameName() const { return m_playniteGameName; }
    void setComputer(NvComputer *computer) { m_computer = computer; }

    // Integration with other managers
    void setServerCommandManager(ServerCommandManager *manager);
    void setClipboardManager(ClipboardManager *manager);

    // Window management
    void setWindow(QWindow *window);
    void setWindowGeometry(int x, int y, int width, int height);

    ServerCommandManager* serverCommandManager() const { return m_serverCommandManager; }

signals:
    void visibilityChanged();
    void serverCommandsChanged();
    void fullscreenChanged();
    void mouseCaptureChanged();
    void keyboardCaptureChanged();
    void statsVisibilityChanged();
    void playniteGameChanged();

    // Action signals
    void disconnectRequested();
    void quitRequested();
    void serverCommandsRequested();
    void clipboardUploadRequested();
    void clipboardFetchRequested();
    void statsToggleRequested();
    void mouseCaptureToggleRequested();
    void keyboardCaptureToggleRequested();
    void fullscreenToggleRequested();

private slots:
    void onServerCommandsChanged();
    void onFullscreenChanged();
    void onMouseCaptureChanged();
    void onKeyboardCaptureChanged();
    void onStatsVisibilityChanged();

private:
    void createQuickView();
    void updateQuickView();
    void sendKeyCombo(int keyCombo);
    void refreshPlayniteGame();

    NvComputer *m_computer = nullptr;
    QString m_playniteGameName;

    bool m_isVisible;
    QWindow *m_window;
    QQuickView *m_quickView;
    QQuickItem *m_quickMenuItem;
    
    ServerCommandManager *m_serverCommandManager;
    ClipboardManager *m_clipboardManager;
    
    // State tracking
    bool m_isFullscreen;
    bool m_isMouseCaptured;
    bool m_isKeyboardCaptured;
    bool m_isStatsVisible;
    
    // Window geometry fallback when no QWindow is available
int m_windowX, m_windowY, m_windowWidth, m_windowHeight;
    bool m_hasWindowGeometry;

    QQuickView *m_ToastWindow; // Toast window for notifications
};
